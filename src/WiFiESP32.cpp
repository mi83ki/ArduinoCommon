/**
 * @file WiFiESP32.cpp
 * @brief ESP32用WiFi制御クラス
 * @author Tatsuya Miyazaki
 * @date 2023/5/5
 */

#include "WiFiESP32.h"

#include <esp_system.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr uint32_t kRtcAccessPointMagic = 0x57494649;
constexpr int32_t kMinimumWiFiChannel = 1;
constexpr int32_t kMaximumWiFiChannel = 14;

struct LastSuccessfulAccessPoint {
  uint32_t magic;
  char ssid[33];
  uint8_t bssid[6];
  int32_t channel;
};

RTC_DATA_ATTR LastSuccessfulAccessPoint lastSuccessfulAccessPoint = {};

bool hasValidBssid(const uint8_t *bssid) {
  if (bssid == nullptr) return false;
  for (uint8_t i = 0; i < 6; ++i) {
    if (bssid[i] != 0) return true;
  }
  return false;
}

void invalidateLastSuccessfulAccessPoint() {
  lastSuccessfulAccessPoint.magic = 0;
}

}  // namespace

/**
 * @brief WiFi接続設定を1件指定して生成する。
 *
 * 指定したSSIDを通常接続先として保持する。既存利用箇所との後方互換性を
 * 維持するため、コンストラクタのシグネチャは変更しない。
 *
 * @param SSID WiFiのSSID
 * @param PASS WiFiのパスワード
 */
WiFiESP32::WiFiESP32(const char *SSID, const char *PASS)
    : _credentials{{String(SSID == nullptr ? "" : SSID),
                    String(PASS == nullptr ? "" : PASS), true, false,
                    IPAddress(), IPAddress(), IPAddress()}},
      _clientIp(),
      _lastFailedAttemptMs(0),
      _hasFailedAttempt(false) {}

/**
 * @brief WiFi制御クラスを破棄する。
 */
WiFiESP32::~WiFiESP32() {}

/**
 * @brief フォールバック用アクセスポイントを追加する。
 *
 * ESP32 Arduino Core 2.0.17のWiFiMultiが受け付ける長さに合わせて検証し、
 * 同一SSIDの重複登録を防ぐ。
 *
 * @param SSID WiFiのSSID
 * @param PASS WiFiのパスワード。オープンAPの場合は空文字列またはnullptr
 * @return true 登録成功
 * @return false 入力不正、重複、またはWiFiMultiへの登録失敗
 */
bool WiFiESP32::addAP(const char *SSID, const char *PASS) {
  return addCredential(SSID, PASS, false, IPAddress(), IPAddress(),
                       IPAddress());
}

/**
 * @brief 固定IP設定を持つフォールバックアクセスポイントを追加する。
 *
 * @param SSID WiFiのSSID
 * @param PASS WiFiのパスワード
 * @param ipAddress 固定IPアドレス
 * @param gateway ゲートウェイアドレス
 * @param subnet サブネットマスク
 * @return true 登録成功
 * @return false 認証情報またはネットワーク設定が不正、重複、登録失敗
 */
bool WiFiESP32::addAP(const char *SSID, const char *PASS,
                      const char *ipAddress, const char *gateway,
                      const char *subnet) {
  if (ipAddress == nullptr || gateway == nullptr || subnet == nullptr) {
    logger.error("WiFiESP32::addAP(): Network address is null.");
    return false;
  }

  IPAddress parsedIp;
  IPAddress parsedGateway;
  IPAddress parsedSubnet;
  if (!parsedIp.fromString(ipAddress) ||
      !parsedGateway.fromString(gateway) ||
      !parsedSubnet.fromString(subnet)) {
    logger.error("WiFiESP32::addAP(): Invalid network address.");
    return false;
  }

  return addCredential(SSID, PASS, true, parsedIp, parsedGateway,
                       parsedSubnet);
}

/**
 * @brief 検証済みネットワーク設定と認証情報を登録する。
 *
 * @param SSID WiFiのSSID
 * @param PASS WiFiのパスワード
 * @param staticIpEnabled 固定IPを使用する場合はtrue
 * @param staticIp 固定IPアドレス
 * @param gateway ゲートウェイアドレス
 * @param subnet サブネットマスク
 * @return true 登録成功
 * @return false 認証情報が不正、重複、またはWiFiMultiへの登録失敗
 */
bool WiFiESP32::addCredential(const char *SSID, const char *PASS,
                              bool staticIpEnabled,
                              const IPAddress &staticIp,
                              const IPAddress &gateway,
                              const IPAddress &subnet) {
  if (SSID == nullptr || SSID[0] == '\0' || std::strlen(SSID) > 31) {
    logger.error("WiFiESP32::addAP(): Invalid SSID.");
    return false;
  }
  if (PASS != nullptr && std::strlen(PASS) > 64) {
    logger.error("WiFiESP32::addAP(): Invalid password length.");
    return false;
  }
  if (findCredential(SSID) != nullptr) {
    logger.warn("WiFiESP32::addAP(): Duplicate SSID ignored: " + String(SSID));
    return false;
  }

  const char *password =
      PASS == nullptr || PASS[0] == '\0' ? nullptr : PASS;
  if (!_wifiMulti.addAP(SSID, password)) {
    logger.error("WiFiESP32::addAP(): WiFiMulti rejected SSID: " +
                 String(SSID));
    return false;
  }

  _credentials.push_back(
      {String(SSID), String(PASS == nullptr ? "" : PASS), false,
       staticIpEnabled, staticIp, gateway, subnet});
  return true;
}

/**
 * @brief 通常SSIDで使用する固定IP設定を登録する。
 *
 * フォールバック候補ではこの設定を使用せず、DHCPへ切り替える。
 *
 * @param ipAddress 固定IPアドレス
 * @param gateway ゲートウェイアドレス
 * @param subnet サブネットマスク
 * @return true 設定文字列がすべて有効
 * @return false いずれかの設定文字列が無効
 */
bool WiFiESP32::setStaticIp(const char *ipAddress, const char *gateway,
                            const char *subnet) {
  if (ipAddress == nullptr || gateway == nullptr || subnet == nullptr) {
    logger.error("WiFiESP32::setStaticIp(): Network address is null.");
    return false;
  }

  IPAddress parsedIp;
  IPAddress parsedGateway;
  IPAddress parsedSubnet;
  if (!parsedIp.fromString(ipAddress) ||
      !parsedGateway.fromString(gateway) ||
      !parsedSubnet.fromString(subnet)) {
    logger.error("WiFiESP32::setStaticIp(): Invalid network address.");
    return false;
  }

  WiFiCredential &primary = _credentials.front();
  primary.staticIp = parsedIp;
  primary.gateway = parsedGateway;
  primary.subnet = parsedSubnet;
  primary.staticIpEnabled = true;
  return true;
}

/**
 * @brief WiFi接続を1シーケンスだけ実行する。
 *
 * @return true 接続成功
 * @return false 全接続経路の失敗
 */
bool WiFiESP32::begin(void) {
  const bool connected = connectWiFi();
  _hasFailedAttempt = !connected;
  if (connected) return true;

  _lastFailedAttemptMs = millis();
  return false;
}

/**
 * @brief RTC、通常SSID、WiFiMultiの順で接続を試す。
 *
 * @return true いずれかの接続経路で成功
 * @return false 全接続経路で失敗
 */
bool WiFiESP32::connectWiFi(void) {
  const uint32_t startTime = millis();
  WiFi.mode(WIFI_STA);

  if (connectFromRtc() || connectPrimary() || connectFallback()) {
    _clientIp = WiFi.localIP();
    saveConnectedAccessPoint();
    logConnectionResult(millis() - startTime);
    return true;
  }

  String message = "WiFiESP32::connectWiFi(): All connections failed. status=";
  message.concat(String(static_cast<int>(WiFi.status())));
  message.concat(", elapsed_ms=");
  message.concat(String(millis() - startTime));
  logger.error(message);
  return false;
}

/**
 * @brief 指定した認証情報で接続し、結果が確定するまで待つ。
 *
 * @param credential 接続に使用する現在の認証情報
 * @param timeoutMs 接続待機時間
 * @param channel 接続先チャンネル。0の場合は指定しない
 * @param bssid 接続先BSSID。nullptrの場合は指定しない
 * @return true 接続成功
 * @return false 接続失敗またはタイムアウト
 */
bool WiFiESP32::connectWithCredential(const WiFiCredential &credential,
                                      uint32_t timeoutMs, int32_t channel,
                                      const uint8_t *bssid) {
  const char *password = credential.password.length() == 0
                             ? nullptr
                             : credential.password.c_str();
  WiFi.begin(credential.ssid.c_str(), password, channel, bssid);
  return WiFi.waitForConnectResult(timeoutMs) == WL_CONNECTED;
}

/**
 * @brief deep sleep前に成功したアクセスポイントへ高速接続する。
 *
 * RTC情報はdeep sleep復帰時だけ使用し、現在の候補に存在しない場合や
 * 内容が不正な場合は無効化する。
 *
 * @return true RTC情報を使用した接続成功
 * @return false RTC情報が利用不可、または接続失敗
 */
bool WiFiESP32::connectFromRtc(void) {
  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) return false;
  if (lastSuccessfulAccessPoint.magic != kRtcAccessPointMagic ||
      std::memchr(lastSuccessfulAccessPoint.ssid, '\0',
                  sizeof(lastSuccessfulAccessPoint.ssid)) == nullptr ||
      lastSuccessfulAccessPoint.channel < kMinimumWiFiChannel ||
      lastSuccessfulAccessPoint.channel > kMaximumWiFiChannel ||
      !hasValidBssid(lastSuccessfulAccessPoint.bssid)) {
    invalidateLastSuccessfulAccessPoint();
    return false;
  }

  const WiFiCredential *credential =
      findCredential(lastSuccessfulAccessPoint.ssid);
  if (credential == nullptr || !configureNetwork(*credential)) {
    invalidateLastSuccessfulAccessPoint();
    return false;
  }

  logger.info("WiFiESP32::connectFromRtc(): Trying SSID: " +
              credential->ssid);
  if (connectWithCredential(*credential, WIFI_FAST_CONNECT_WAIT,
                            lastSuccessfulAccessPoint.channel,
                            lastSuccessfulAccessPoint.bssid)) {
    return true;
  }

  invalidateLastSuccessfulAccessPoint();
  return false;
}

/**
 * @brief EEPROM由来の通常SSIDへ直接接続する。
 *
 * @return true 接続成功
 * @return false 候補不正、ネットワーク設定失敗、または接続失敗
 */
bool WiFiESP32::connectPrimary(void) {
  if (_credentials.empty() || _credentials.front().ssid.length() == 0) {
    logger.error("WiFiESP32::connectPrimary(): Primary SSID is empty.");
    return false;
  }

  const WiFiCredential &credential = _credentials.front();
  if (!configureNetwork(credential)) return false;
  logger.info("WiFiESP32::connectPrimary(): Trying SSID: " +
              credential.ssid);
  return connectWithCredential(credential, WIFI_PRIMARY_CONNECT_WAIT);
}

/**
 * @brief WiFiMultiでフォールバック候補へ接続する。
 *
 * @return true 接続成功
 * @return false 候補なし、DHCP設定失敗、または接続失敗
 */
bool WiFiESP32::connectFallback(void) {
  if (_credentials.size() <= 1) return false;

  disconnectWiFi();
  if (hasStaticFallback()) return connectFallbackFromScan();

  if (!enableDhcp()) {
    logger.error("WiFiESP32::connectFallback(): Failed to enable DHCP.");
    return false;
  }

  logger.info("WiFiESP32::connectFallback(): Scanning fallback SSIDs.");
  return _wifiMulti.run(WIFI_TRY_WAIT) == WL_CONNECTED;
}

/**
 * @brief スキャン結果から登録済みフォールバック候補へRSSI順に接続する。
 *
 * スキャンは1回だけ実行し、同一SSIDが複数見つかった場合は最もRSSIが強い
 * BSSIDを使用する。各候補への接続前に、その候補固有のIP設定を適用する。
 *
 * @return true いずれかの候補への接続成功
 * @return false スキャン失敗、候補なし、または全候補への接続失敗
 */
bool WiFiESP32::connectFallbackFromScan(void) {
  logger.info("WiFiESP32::connectFallbackFromScan(): Scanning fallback "
              "SSIDs.");
  const int16_t networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) {
    WiFi.scanDelete();
    return false;
  }

  std::vector<ScannedAccessPoint> candidates;
  for (int16_t index = 0; index < networkCount; ++index) {
    String ssid;
    uint8_t encryptionType = 0;
    int32_t rssi = 0;
    uint8_t *bssid = nullptr;
    int32_t channel = 0;
    if (!WiFi.getNetworkInfo(static_cast<uint8_t>(index), ssid,
                             encryptionType, rssi, bssid, channel)) {
      continue;
    }

    const WiFiCredential *credential = findCredential(ssid.c_str());
    if (credential == nullptr || credential->primary ||
        !hasValidBssid(bssid) || channel < kMinimumWiFiChannel ||
        channel > kMaximumWiFiChannel) {
      continue;
    }

    const auto existing = std::find_if(
        candidates.begin(), candidates.end(),
        [credential](const ScannedAccessPoint &candidate) {
          return candidate.credential == credential;
        });
    if (existing != candidates.end() && existing->rssi >= rssi) continue;

    ScannedAccessPoint accessPoint{
        credential, rssi, {{0, 0, 0, 0, 0, 0}}, channel};
    std::copy(bssid, bssid + accessPoint.bssid.size(),
              accessPoint.bssid.begin());
    if (existing == candidates.end()) {
      candidates.push_back(accessPoint);
    } else {
      *existing = accessPoint;
    }
  }
  WiFi.scanDelete();

  std::sort(candidates.begin(), candidates.end(),
            [](const ScannedAccessPoint &left,
               const ScannedAccessPoint &right) {
              return left.rssi > right.rssi;
            });

  for (const ScannedAccessPoint &candidate : candidates) {
    if (!configureNetwork(*candidate.credential)) continue;
    logger.info("WiFiESP32::connectFallbackFromScan(): Trying SSID: " +
                candidate.credential->ssid);
    if (connectWithCredential(*candidate.credential, WIFI_TRY_WAIT,
                              candidate.channel, candidate.bssid.data())) {
      return true;
    }
    disconnectWiFi();
  }
  return false;
}

/**
 * @brief 接続候補の種別に応じて固定IPまたはDHCPを設定する。
 *
 * @param credential 接続対象の認証情報
 * @return true ネットワーク設定成功
 * @return false ネットワーク設定失敗
 */
bool WiFiESP32::configureNetwork(const WiFiCredential &credential) {
  if (credential.staticIpEnabled) {
    return WiFi.config(credential.staticIp, credential.gateway,
                       credential.subnet);
  }
  return enableDhcp();
}

/**
 * @brief WiFiインターフェースをDHCPへ戻す。
 *
 * @return true DHCP設定成功
 * @return false DHCP設定失敗
 */
bool WiFiESP32::enableDhcp(void) {
  const IPAddress dynamicAddress(INADDR_NONE);
  return WiFi.config(dynamicAddress, dynamicAddress, dynamicAddress);
}

/**
 * @brief 固定IPを使用するフォールバック候補があるか判定する。
 *
 * @return true 固定IP候補あり
 * @return false 全フォールバック候補がDHCP
 */
bool WiFiESP32::hasStaticFallback(void) const {
  return std::any_of(
      _credentials.begin(), _credentials.end(),
      [](const WiFiCredential &credential) {
        return !credential.primary && credential.staticIpEnabled;
      });
}

/**
 * @brief 現在の候補からSSIDが一致する認証情報を探す。
 *
 * @param SSID 検索するSSID
 * @return const WiFiCredential* 一致した候補。存在しない場合はnullptr
 */
const WiFiESP32::WiFiCredential *WiFiESP32::findCredential(
    const char *SSID) const {
  if (SSID == nullptr) return nullptr;
  for (const WiFiCredential &credential : _credentials) {
    if (credential.ssid == SSID) return &credential;
  }
  return nullptr;
}

/**
 * @brief 現在の接続情報をdeep sleep保持領域へ保存する。
 */
void WiFiESP32::saveConnectedAccessPoint(void) {
  const String connectedSsid = WiFi.SSID();
  const uint8_t *bssid = WiFi.BSSID();
  const int32_t channel = WiFi.channel();
  if (connectedSsid.length() == 0 || connectedSsid.length() > 32 ||
      findCredential(connectedSsid.c_str()) == nullptr ||
      !hasValidBssid(bssid) || channel < kMinimumWiFiChannel ||
      channel > kMaximumWiFiChannel) {
    invalidateLastSuccessfulAccessPoint();
    return;
  }

  lastSuccessfulAccessPoint = {};
  lastSuccessfulAccessPoint.magic = kRtcAccessPointMagic;
  std::strncpy(lastSuccessfulAccessPoint.ssid, connectedSsid.c_str(),
               sizeof(lastSuccessfulAccessPoint.ssid) - 1);
  std::memcpy(lastSuccessfulAccessPoint.bssid, bssid,
              sizeof(lastSuccessfulAccessPoint.bssid));
  lastSuccessfulAccessPoint.channel = channel;
}

/**
 * @brief 接続先と所要時間をログ出力する。
 *
 * @param elapsedMs 接続シーケンスの所要時間
 */
void WiFiESP32::logConnectionResult(uint32_t elapsedMs) const {
  String message = "WiFi connected: ssid=";
  message.concat(WiFi.SSID());
  message.concat(", ip=");
  message.concat(_clientIp.toString());
  message.concat(", rssi=");
  message.concat(String(WiFi.RSSI()));
  message.concat(", elapsed_ms=");
  message.concat(String(elapsedMs));
  logger.info(message);
}

/**
 * @brief WiFi接続を切断する。
 */
void WiFiESP32::disconnectWiFi(void) {
  WiFi.disconnect(false, false);
  logger.info("WiFi Disconnected.");
}

/**
 * @brief WiFiが接続されているか判定する。
 *
 * @return true 接続中
 * @return false 切断中
 */
bool WiFiESP32::isConnected(void) {
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief 現在接続中のWiFi SSIDを取得する。
 *
 * @return String 接続中のSSID。未接続の場合は空文字列
 */
String WiFiESP32::getConnectedSsid(void) const {
  return WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String();
}

/**
 * @brief WiFi接続を確認し、切断中はバックオフ後に再接続する。
 *
 * @return true 接続中または再接続成功
 * @return false 切断中、バックオフ中、または再接続失敗
 */
bool WiFiESP32::healthCheck(void) {
  if (isConnected()) {
    _hasFailedAttempt = false;
    return true;
  }
  if (_hasFailedAttempt &&
      millis() - _lastFailedAttemptMs < WIFI_RECONNECT_INTERVAL) {
    return false;
  }

  const bool connected = connectWiFi();
  _hasFailedAttempt = !connected;
  if (!connected) _lastFailedAttemptMs = millis();
  return connected;
}
