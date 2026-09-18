/**
 * @file WiFiProvisioningProbe.cpp
 * @brief 設定用APを維持しながらWi-Fi接続試験とスキャンを実行する実装。
 */

#if !defined(ARDUINOCOMMON_DISABLE_PROVISIONING) && (defined(ARDUINO_ARCH_ESP32) || defined(ARDUINOCOMMON_TEST_ESP32))
#include "WiFiProvisioningProbe.h"
#include <WiFi.h>
#include <esp_netif.h>
#include <lwip/dns.h>
#include <esp_wifi.h>
#include <algorithm>

namespace ArduinoCommon {
namespace {
/**
 * @brief SDKのIP型へネットワーク順で変換する。
 * @param ip 変換する4バイトIPv4アドレス
 * @return IPAddress SDKのIPアドレスオブジェクト
 */
IPAddress ipAddress(const IPv4& ip) {return IPAddress(ip[0],ip[1],ip[2],ip[3]);}
/**
 * @brief SDKのIP型を4バイトのIPv4値へ変換する。
 * @param ip 変換するSDKのIPアドレス
 * @return IPv4 4バイトのIPv4アドレス
 */
IPv4 bytes(const IPAddress& ip) {return {{ip[0],ip[1],ip[2],ip[3]}};}
/**
 * @brief APの/24とSTAのサブネットが重なるかを判定する。
 * @param ap 設定用APのIPv4アドレス
 * @param sta STA側のIPv4アドレス
 * @param mask STA側のサブネットマスク
 * @return true 両者のネットワークが重なる場合
 * @return false 重ならない場合
 */
bool overlaps(const IPv4& ap,const IPv4& sta,const IPv4& mask) {
  for(size_t i=0;i<4;++i) {
    const uint8_t common=mask[i] & (i<3?255:0);
    if((ap[i]&common)!=(sta[i]&common))return false;
  }
  return true;
}
/**
 * @brief IDFのゼロDNS拒否を避け、TCP/IPタスクで未使用DNSを同期的に解除する。
 * @param first DNS1を解除する場合はtrue
 * @param second DNS2を解除する場合はtrue
 * @return true DNSを解除できた場合
 * @return false STAインターフェースまたはTCP/IP処理を取得できない場合
 */
bool clearDns(bool first,bool second) {
  if(!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"))return false;
  bool clear[]={first,second};
  return esp_netif_tcpip_exec([](void* context)->esp_err_t {
    const auto* slots=static_cast<const bool*>(context);
    if(slots[0])dns_setserver(ESP_NETIF_DNS_MAIN,nullptr);
    if(slots[1])dns_setserver(ESP_NETIF_DNS_BACKUP,nullptr);
    dns_setserver(ESP_NETIF_DNS_FALLBACK,nullptr);
    return ESP_OK;
  },clear)==ESP_OK;
}
}

/**
 * @brief 設定用APを開始する。通常WiFiESP32と並行せず、同一所有タスクから呼ぶ。
 * @param credentials 設定用APの認証情報
 * @param apAddress 設定用APに割り当てるIPv4アドレス
 * @return true APを開始できた場合
 * @return false 引数またはAP初期化が不正な場合
 */
bool WiFiProvisioningProbe::begin(const ApCredentials& credentials,IPv4 apAddress) {
  if(_started || credentials.ssid.empty() || credentials.ssid.size()>31 ||
      !WiFiProfileValidator::validUtf8(credentials.ssid) || credentials.password.size()<8 ||
      credentials.password.size()>63 || !WiFiProfileValidator::validPassword(credentials.password))return false;
  _credentials=credentials;_apAddress=apAddress;
  WiFi.persistent(false);WiFi.mode(WIFI_AP_STA);WiFi.setAutoReconnect(false);
  WiFi.disconnect(false,false);
  _started=restoreAp();return _started;
}
/**
 * @brief 同じ資格情報と固定アドレスでAPを復帰する。
 * @return true APを復帰できた場合
 * @return false AP設定または開始に失敗した場合
 */
bool WiFiProvisioningProbe::restoreAp() {
  _apAvailable=false;
  WiFi.mode(WIFI_AP_STA);
  const auto ip=ipAddress(_apAddress);
  if(!WiFi.softAPConfig(ip,ip,IPAddress(255,255,255,0)) ||
      !WiFi.softAP(_credentials.ssid.c_str(),_credentials.password.c_str(),1,0,1))return false;
  _result.apSuspended=false;_apAvailable=true;return true;
}
/** @brief スキャンと接続試験を停止し、無線の所有を返す。HTTPタスク外で呼ぶ。 */
void WiFiProvisioningProbe::stop() {
  _apAvailable=false;
  if(_scanState==WiFiScanState::Scanning)esp_wifi_scan_stop();
  WiFi.scanDelete();WiFi.disconnect(false,false);WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);
  _started=false;_stationOwned=false;_scanState=WiFiScanState::Idle;_scanResults.clear();_result={};
}
/**
 * @brief 指定SSIDだけを非同期に試験する。deadlineはmillis基準で最大20秒先とする。
 * @param profile 接続試験するWi-Fiプロファイル
 * @param jobId 試験を識別するジョブID。0は無効
 * @param deadlineMillis 試験を終了する絶対時刻
 * @return true 試験を開始できた場合
 * @return false 既存試験中、期限不正、またはプロファイル不正の場合
 */
bool WiFiProvisioningProbe::start(const WiFiProfile& profile,uint64_t jobId,uint32_t deadlineMillis) {
  const uint32_t timeout=deadlineMillis-millis();
  if(!_started || _stationOwned || _scanState==WiFiScanState::Scanning || !jobId ||
      timeout==0 || timeout>20000 || !WiFiProfileValidator::validate({profile},profile.id,true).empty())return false;
  _profile=profile;_startedAt=millis();_timeout=timeout;_stationOwned=true;
  _result={};_result.jobId=jobId;_result.profileId=profile.id;_result.state=WiFiProbeState::Connecting;
  WiFi.disconnect(false,false);
  if(profile.staticIp && overlaps(_apAddress,profile.ip,profile.mask)) {
    _apAvailable=false;
    if(!WiFi.enableAP(false)) {fail("ap_stop_failed");return true;}
    _result.apSuspended=true;
  }
  bool configured;
  if(profile.staticIp) {
    configured=WiFi.config(ipAddress(profile.ip),ipAddress(profile.gateway),ipAddress(profile.mask),
        ipAddress(profile.dns1),ipAddress(profile.dns2)) &&
        clearDns(profile.dns1==IPv4{},profile.dns2==IPv4{});
  } else {
    configured=WiFi.config(IPAddress(INADDR_NONE),IPAddress(INADDR_NONE),IPAddress(INADDR_NONE)) && clearDns(true,true);
  }
  if(!configured) {fail("network_config_failed");return true;}
  WiFi.begin(_profile.ssid.c_str(),_profile.password.empty()?nullptr:_profile.password.c_str());
  return true;
}
/**
 * @brief 試験失敗時はSTAを解除し、候補結果を保持したままAPへ戻す。
 * @param error 失敗理由を表す内部コード
 */
void WiFiProvisioningProbe::fail(const char* error) {
  _result.state=WiFiProbeState::Failed;_result.error=error;
  WiFi.disconnect(false,false);_stationOwned=false;
  if(!restoreAp())_result.error="ap_restore_failed";
}
/** @brief Wi-Fi/IPとスキャンの進行を待機なしで観測する。MQTTや製品保存は扱わない。 */
void WiFiProvisioningProbe::poll() {
  if(!_started)return;
  if(_scanState==WiFiScanState::Scanning) {
    // IDFで開始した検索もArduinoの完了イベントが結果を保持する。未完了時は
    // scanComplete()の古い開始時刻を参照せず、この所有者の総期限だけを使う。
    const bool expired=uint32_t(millis()-_scanAt)>=10000;
    const int count=expired?WIFI_SCAN_FAILED:
        (WiFi.getStatusBits()&WIFI_SCAN_DONE_BIT)?WiFi.scanComplete():WIFI_SCAN_RUNNING;
    if(count>=0) {
      _scanResults.clear();
      for(int index=0;index<count && index<256;++index) {
        String ssid;uint8_t encryption=0,*bssid=nullptr;int32_t rssi=0,channel=0;
        if(!WiFi.getNetworkInfo(uint8_t(index),ssid,encryption,rssi,bssid,channel) ||
            ssid.length()==0 || ssid.length()>31 || !WiFiProfileValidator::validUtf8(ssid.c_str()))continue;
        auto found=std::find_if(_scanResults.begin(),_scanResults.end(),[&](const WiFiScanEntry& entry){return entry.ssid==ssid.c_str();});
        if(found!=_scanResults.end()) {if(rssi>found->rssi) {found->rssi=rssi;found->open=encryption==0;}}
        else {WiFiScanEntry entry;entry.ssid=ssid.c_str();entry.rssi=rssi;entry.open=encryption==0;_scanResults.push_back(entry);}
        std::sort(_scanResults.begin(),_scanResults.end(),[](const WiFiScanEntry& a,const WiFiScanEntry& b){return a.rssi>b.rssi;});
        if(_scanResults.size()>20)_scanResults.pop_back();
      }
      WiFi.scanDelete();_scanCompletedAt=millis();_scanState=WiFiScanState::Ready;
    } else if(count==WIFI_SCAN_FAILED || uint32_t(millis()-_scanAt)>=10000) {
      esp_wifi_scan_stop();WiFi.scanDelete();_scanState=WiFiScanState::Failed;
    }
  }
  if(_result.state!=WiFiProbeState::Connecting)return;
  if(uint32_t(millis()-_startedAt)>=_timeout) {fail("wifi_timeout");return;}
  const auto status=WiFi.status();
  if(status==WL_CONNECT_FAILED || status==WL_NO_SSID_AVAIL) {fail("wifi_unavailable");return;}
  if(status!=WL_CONNECTED || WiFi.SSID()!=_profile.ssid.c_str())return;
  const auto address=bytes(WiFi.localIP());
  if(address==IPv4{} || (_profile.staticIp && address!=_profile.ip))return;
  if(!_result.apSuspended && overlaps(_apAddress,address,bytes(WiFi.subnetMask()))) {
    _apAvailable=false;
    if(!WiFi.enableAP(false)) {fail("ap_stop_failed");return;}
    _result.apSuspended=true;
  }
  _result.address=address;_result.state=WiFiProbeState::Succeeded;
}
/**
 * @brief ジョブIDが一致する試験だけを取り消し、遅い結果の採用を防ぐ。
 * @param jobId 取り消す試験のジョブID
 * @return true 試験を取り消した場合
 * @return false 対象ジョブが存在しない、または試験中でない場合
 */
bool WiFiProvisioningProbe::cancel(uint64_t jobId) {
  if(!_stationOwned || _result.jobId!=jobId)return false;
  WiFi.disconnect(false,false);_stationOwned=false;_result.state=WiFiProbeState::Cancelled;
  if(!restoreAp()) {_result.state=WiFiProbeState::Failed;_result.error="ap_restore_failed";}
  return true;
}
/**
 * @brief 利用側の診断終了後、STAを停止してAPを復帰する。
 * @param jobId 終了する試験のジョブID
 * @return true 試験を終了してAPを復帰できた場合
 * @return false 対象ジョブが未完了、または復帰に失敗した場合
 */
bool WiFiProvisioningProbe::finish(uint64_t jobId) {
  if(!_stationOwned || _result.jobId!=jobId || _result.state==WiFiProbeState::Connecting)return false;
  WiFi.disconnect(false,false);_stationOwned=false;
  if(!restoreAp()) {_result.error="ap_restore_failed";return false;}
  return true;
}
/**
 * @brief 結果を所有コピーで返す。公開APIの呼出しは同一所有タスクへ直列化する。
 * @return WiFiProbeResult 現在の接続試験結果
 */
WiFiProbeResult WiFiProvisioningProbe::result() const {return _result;}
/**
 * @brief HTTP側からSDKに触れず、APが利用できるかを原子的に確認する。
 * @return true 設定用APが利用可能な場合
 * @return false APが停止中または利用不能な場合
 */
bool WiFiProvisioningProbe::apAvailable() const {return _apAvailable;}
/**
 * @brief 接続試験中のスキャンを拒否し、30秒以内の結果は再利用する。
 * @return true スキャンを開始した、または有効なキャッシュを再利用した場合
 * @return false スキャンを開始できない場合
 */
bool WiFiProvisioningProbe::startScan() {
  if(!_started || _stationOwned || _scanState==WiFiScanState::Scanning)return false;
  if(_scanState==WiFiScanState::Ready && uint32_t(millis()-_scanCompletedAt)<30000)return true;
  _scanResults.clear();_scanAt=millis();_scanState=WiFiScanState::Scanning;
  // 固定core 2.0.17のscanNetworksはhome_chan_dwell_timeを初期化しないため、
  // IDFの公開APIへ全項目を初期化して渡す。非同期完了・結果保持はArduinoへ任せる。
  WiFi.scanDelete();wifi_scan_config_t config{};
  config.show_hidden=true;config.scan_type=WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min=100;config.scan_time.active.max=300;config.home_chan_dwell_time=30;
  if(esp_wifi_scan_start(&config,false)!=ESP_OK) {_scanState=WiFiScanState::Failed;return false;}
  return true;
}
/**
 * @brief Wi-Fiスキャンの現在状態を返す。
 * @return WiFiScanState 現在のスキャン状態
 */
WiFiScanState WiFiProvisioningProbe::scanState() const {return _scanState;}
/**
 * @brief 最新スキャン結果を呼出元へ所有コピーで返す。
 * @return std::vector<WiFiScanEntry> SSIDごとに整理したスキャン結果
 */
std::vector<WiFiScanEntry> WiFiProvisioningProbe::scanResults() const {return _scanResults;}
}
#endif
