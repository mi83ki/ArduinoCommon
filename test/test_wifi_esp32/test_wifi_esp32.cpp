#include <unity.h>

#include <array>
#include <cstring>
#include <type_traits>

#include "FakeLogState.h"
#include "WiFi.h"
#include "WiFiESP32.h"

void setUp(void) {
  FakeWiFiState::reset();
  FakeLogState::reset();
}

void tearDown(void) {}

/**
 * @brief 既存コンストラクタのSSIDとパスワードで通常接続することを検証する。
 */
void test_legacy_constructor_uses_primary_credentials(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("primary",
                           FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("primary-password",
                           FakeWiFiState::beginCalls[0].password.c_str());
}

/**
 * @brief WiFiMultiの所有データを浅くコピーしないようコピー禁止を検証する。
 */
void test_wifi_esp32_is_not_copyable(void) {
  TEST_ASSERT_FALSE(std::is_copy_constructible<WiFiESP32>::value);
  TEST_ASSERT_FALSE(std::is_copy_assignable<WiFiESP32>::value);
}

/**
 * @brief 有効なフォールバック候補をWiFiMultiへ登録できることを検証する。
 */
void test_add_ap_registers_valid_fallback(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::addedAps.size());
  TEST_ASSERT_EQUAL_STRING("fallback",
                           FakeWiFiState::addedAps[0].first.c_str());
  TEST_ASSERT_EQUAL_STRING("fallback-password",
                           FakeWiFiState::addedAps[0].second.c_str());
}

/**
 * @brief 固定IP付きフォールバック候補を登録できることを検証する。
 */
void test_add_ap_registers_static_network_profile(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password",
                              "192.168.2.50", "192.168.2.1",
                              "255.255.255.0"));
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::addedAps.size());
}

/**
 * @brief 固定IP付き候補の不正なネットワーク設定を拒否することを検証する。
 */
void test_add_ap_rejects_invalid_static_network_profile(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(wifi.addAP("fallback", "fallback-password", "invalid",
                               "192.168.2.1", "255.255.255.0"));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::addedAps.size());
}

/**
 * @brief 空SSIDをフォールバック候補として拒否することを検証する。
 */
void test_add_ap_rejects_empty_ssid(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(wifi.addAP("", "password"));
  TEST_ASSERT_FALSE(wifi.addAP(nullptr, "password"));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::addedAps.size());
}

/**
 * @brief WiFiMultiが扱えない32文字SSIDを拒否することを検証する。
 */
void test_add_ap_rejects_32_character_ssid(void) {
  const char* ssid = "12345678901234567890123456789012";
  TEST_ASSERT_EQUAL_UINT32(32, std::strlen(ssid));
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(wifi.addAP(ssid, "password"));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::addedAps.size());
}

/**
 * @brief 64文字を超えるパスワードを拒否することを検証する。
 */
void test_add_ap_rejects_password_longer_than_64_characters(void) {
  const char* password =
      "12345678901234567890123456789012345678901234567890123456789012345";
  TEST_ASSERT_EQUAL_UINT32(65, std::strlen(password));
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(wifi.addAP("fallback", password));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::addedAps.size());
}

/**
 * @brief 通常SSIDと同じフォールバックSSIDを拒否することを検証する。
 */
void test_add_ap_rejects_duplicate_primary_ssid(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(wifi.addAP("primary", "another-password"));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::addedAps.size());
}

/**
 * @brief 登録済みフォールバックSSIDの重複登録を拒否することを検証する。
 */
void test_add_ap_rejects_duplicate_fallback_ssid(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(wifi.addAP("fallback", "first-password"));
  TEST_ASSERT_FALSE(wifi.addAP("fallback", "second-password"));
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::addedAps.size());
  TEST_ASSERT_EQUAL_STRING("first-password",
                           FakeWiFiState::addedAps[0].second.c_str());
}

static void seedFallbackRtcRecord(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setMultiResult(WL_CONNECTED, "fallback", 100, 6,
                                {{0x10, 0x20, 0x30, 0x40, 0x50, 0x60}});
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));
  TEST_ASSERT_TRUE(wifi.begin());
}

/**
 * @brief deep sleep復帰時に前回成功したAPへ直接接続することを検証する。
 */
void test_deep_sleep_uses_last_successful_ap_fast_path(void) {
  seedFallbackRtcRecord();

  FakeWiFiState::reset();
  FakeWiFiState::resetReason = ESP_RST_DEEPSLEEP;
  FakeWiFiState::setDirectResult("fallback", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("fallback",
                           FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_TRUE(FakeWiFiState::beginCalls[0].hasBssid);
  TEST_ASSERT_EQUAL_INT32(6, FakeWiFiState::beginCalls[0].channel);
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::multiRunCalls);
}

/**
 * @brief 現在の候補に存在しないRTCのSSIDを無視することを検証する。
 */
void test_deep_sleep_ignores_rtc_ssid_not_in_current_candidates(void) {
  FakeWiFiState::setDirectResult("old-primary", WL_CONNECTED);
  {
    WiFiESP32 oldWifi("old-primary", "old-password");
    TEST_ASSERT_TRUE(oldWifi.begin());
  }

  FakeWiFiState::reset();
  FakeWiFiState::resetReason = ESP_RST_DEEPSLEEP;
  FakeWiFiState::setDirectResult("new-primary", WL_CONNECTED);
  WiFiESP32 wifi("new-primary", "new-password");

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("new-primary",
                           FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_FALSE(FakeWiFiState::beginCalls[0].hasBssid);
}

/**
 * @brief RTC高速接続失敗後に通常SSIDを直接試すことを検証する。
 */
void test_failed_fast_path_falls_back_to_primary(void) {
  seedFallbackRtcRecord();

  FakeWiFiState::reset();
  FakeWiFiState::resetReason = ESP_RST_DEEPSLEEP;
  FakeWiFiState::setDirectResult("fallback", WL_CONNECT_FAILED);
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(2, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("fallback",
                           FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_TRUE(FakeWiFiState::beginCalls[0].hasBssid);
  TEST_ASSERT_EQUAL_STRING("primary",
                           FakeWiFiState::beginCalls[1].ssid.c_str());
  TEST_ASSERT_FALSE(FakeWiFiState::beginCalls[1].hasBssid);
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::multiRunCalls);
}

/**
 * @brief 通常SSID成功時にWiFiMultiを実行しないことを検証する。
 */
void test_primary_success_skips_wifi_multi(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::multiRunCalls);
}

/**
 * @brief 通常SSID失敗後にDHCPへ戻してWiFiMultiを実行することを検証する。
 */
void test_primary_failure_enables_dhcp_and_runs_wifi_multi(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setMultiResult(WL_CONNECTED, "fallback");
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::multiRunCalls);
  TEST_ASSERT_GREATER_THAN_UINT32(0, FakeWiFiState::configCalls.size());
  const FakeWiFiState::ConfigCall& dhcpCall =
      FakeWiFiState::configCalls.back();
  TEST_ASSERT_EQUAL_UINT8(0, dhcpCall.ip[0]);
  TEST_ASSERT_EQUAL_UINT8(0, dhcpCall.ip[1]);
  TEST_ASSERT_EQUAL_UINT8(0, dhcpCall.ip[2]);
  TEST_ASSERT_EQUAL_UINT8(0, dhcpCall.ip[3]);
}

/**
 * @brief 固定IP候補を含む場合は一度だけスキャンしてRSSI順に接続することを検証する。
 */
void test_static_fallback_scans_once_and_tries_visible_profiles_by_rssi(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setDirectResult("strong-static", WL_CONNECT_FAILED);
  FakeWiFiState::setDirectResult("weak-dhcp", WL_CONNECTED);
  FakeWiFiState::addScanNetwork(
      "weak-dhcp", -70, 1, {{0x11, 0x12, 0x13, 0x14, 0x15, 0x16}});
  FakeWiFiState::addScanNetwork(
      "unregistered", -20, 3, {{0x21, 0x22, 0x23, 0x24, 0x25, 0x26}});
  FakeWiFiState::addScanNetwork(
      "strong-static", -35, 6, {{0x31, 0x32, 0x33, 0x34, 0x35, 0x36}});
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("weak-dhcp", "weak-password"));
  TEST_ASSERT_TRUE(wifi.addAP("strong-static", "strong-password",
                              "192.168.2.50", "192.168.2.1",
                              "255.255.255.0"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::scanCalls);
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::scanDeleteCalls);
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::multiRunCalls);
  TEST_ASSERT_EQUAL_UINT32(3, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("strong-static",
                           FakeWiFiState::beginCalls[1].ssid.c_str());
  TEST_ASSERT_EQUAL_INT32(6, FakeWiFiState::beginCalls[1].channel);
  TEST_ASSERT_TRUE(FakeWiFiState::beginCalls[1].hasBssid);
  TEST_ASSERT_EQUAL_STRING("weak-dhcp",
                           FakeWiFiState::beginCalls[2].ssid.c_str());
  TEST_ASSERT_EQUAL_INT32(1, FakeWiFiState::beginCalls[2].channel);
  TEST_ASSERT_TRUE(FakeWiFiState::beginCalls[2].hasBssid);

  TEST_ASSERT_EQUAL_UINT32(3, FakeWiFiState::configCalls.size());
  TEST_ASSERT_TRUE(FakeWiFiState::configCalls[1].ip ==
                   IPAddress(192, 168, 2, 50));
  TEST_ASSERT_TRUE(FakeWiFiState::configCalls[2].ip == IPAddress(0));
}

/**
 * @brief 同一SSIDが複数見つかった場合は最強RSSIのBSSIDだけを試すことを検証する。
 */
void test_static_fallback_uses_strongest_bssid_for_duplicate_ssid(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setDirectResult("fallback", WL_CONNECTED);
  FakeWiFiState::addScanNetwork(
      "fallback", -80, 1, {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10}});
  FakeWiFiState::addScanNetwork(
      "fallback", -40, 11, {{0x20, 0x20, 0x20, 0x20, 0x20, 0x20}});
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password",
                              "192.168.2.50", "192.168.2.1",
                              "255.255.255.0"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(2, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_INT32(11, FakeWiFiState::beginCalls[1].channel);
  TEST_ASSERT_EQUAL_UINT8(0x20, FakeWiFiState::beginCalls[1].bssid[0]);
}

/**
 * @brief 固定IPフォールバック成功後のdeep sleep復帰で同じ固定IPを適用することを検証する。
 */
void test_deep_sleep_applies_static_network_for_last_fallback(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setDirectResult("fallback", WL_CONNECTED);
  FakeWiFiState::addScanNetwork(
      "fallback", -40, 6, {{0x30, 0x31, 0x32, 0x33, 0x34, 0x35}});
  {
    WiFiESP32 wifi("primary", "primary-password");
    TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password",
                                "192.168.3.50", "192.168.3.1",
                                "255.255.255.0"));
    TEST_ASSERT_TRUE(wifi.begin());
  }

  FakeWiFiState::reset();
  FakeWiFiState::resetReason = ESP_RST_DEEPSLEEP;
  FakeWiFiState::setDirectResult("fallback", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password",
                              "192.168.3.50", "192.168.3.1",
                              "255.255.255.0"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("fallback",
                           FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_TRUE(FakeWiFiState::configCalls[0].ip ==
                   IPAddress(192, 168, 3, 50));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::scanCalls);
}

/**
 * @brief 接続後のSSIDを公開APIから取得できることを検証する。
 */
void test_get_connected_ssid_returns_current_wifi_ssid(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_STRING("primary", wifi.getConnectedSsid().c_str());
}

/**
 * @brief フォールバック成功情報が次のdeep sleep復帰で使われることを検証する。
 */
void test_fallback_success_is_saved_for_next_deep_sleep(void) {
  seedFallbackRtcRecord();

  FakeWiFiState::reset();
  FakeWiFiState::resetReason = ESP_RST_DEEPSLEEP;
  FakeWiFiState::setDirectResult("fallback", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_STRING("fallback",
                           FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_TRUE(FakeWiFiState::beginCalls[0].hasBssid);
}

/**
 * @brief 通常SSIDと全フォールバック候補の失敗時にfalseを返すことを検証する。
 */
void test_all_candidates_failure_returns_false(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setMultiResult(WL_CONNECT_FAILED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password"));

  TEST_ASSERT_FALSE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::multiRunCalls);
}

/**
 * @brief 有効な固定IP設定を通常SSID接続前に適用することを検証する。
 */
void test_static_ip_is_applied_to_primary_connection(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(
      wifi.setStaticIp("192.168.1.50", "192.168.1.1", "255.255.255.0"));
  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_GREATER_THAN_UINT32(0, FakeWiFiState::configCalls.size());
  const FakeWiFiState::ConfigCall& call = FakeWiFiState::configCalls.front();
  TEST_ASSERT_TRUE(call.ip == IPAddress(192, 168, 1, 50));
  TEST_ASSERT_TRUE(call.gateway == IPAddress(192, 168, 1, 1));
  TEST_ASSERT_TRUE(call.subnet == IPAddress(255, 255, 255, 0));
}

/**
 * @brief 不正な固定IP、ゲートウェイ、サブネットを拒否することを検証する。
 */
void test_static_ip_rejects_invalid_address_strings(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(
      wifi.setStaticIp("invalid", "192.168.1.1", "255.255.255.0"));
  TEST_ASSERT_FALSE(
      wifi.setStaticIp("192.168.1.50", "invalid", "255.255.255.0"));
  TEST_ASSERT_FALSE(
      wifi.setStaticIp("192.168.1.50", "192.168.1.1", "invalid"));
}

/**
 * @brief nullの固定IP設定を解析処理へ渡さず拒否することを検証する。
 */
void test_static_ip_rejects_null_before_parsing(void) {
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(
      wifi.setStaticIp(nullptr, "192.168.1.1", "255.255.255.0"));
  TEST_ASSERT_FALSE(
      wifi.setStaticIp("192.168.1.50", nullptr, "255.255.255.0"));
  TEST_ASSERT_FALSE(
      wifi.setStaticIp("192.168.1.50", "192.168.1.1", nullptr));
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::nullAddressParseCalls);
}

/**
 * @brief 接続失敗から10秒未満はhealthCheckが再接続しないことを検証する。
 */
void test_health_check_backs_off_for_ten_seconds_after_failure(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_FALSE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());

  TEST_ASSERT_FALSE(wifi.healthCheck());
  delay(9999);
  TEST_ASSERT_FALSE(wifi.healthCheck());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());
}

/**
 * @brief 接続失敗から10秒経過後はhealthCheckが再接続することを検証する。
 */
void test_health_check_retries_after_ten_seconds(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_FALSE(wifi.begin());
  TEST_ASSERT_EQUAL_UINT32(1, FakeWiFiState::beginCalls.size());

  delay(10000);
  TEST_ASSERT_FALSE(wifi.healthCheck());
  TEST_ASSERT_EQUAL_UINT32(2, FakeWiFiState::beginCalls.size());
}

/**
 * @brief 接続済みの場合はhealthCheckが再接続しないことを検証する。
 */
void test_health_check_returns_immediately_when_connected(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED);
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.begin());
  const uint32_t beginCallCount = FakeWiFiState::beginCalls.size();

  TEST_ASSERT_TRUE(wifi.healthCheck());
  TEST_ASSERT_EQUAL_UINT32(beginCallCount, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_UINT32(0, FakeWiFiState::multiRunCalls);
}

/**
 * @brief 接続ログへWi-Fiパスワードを出力しないことを検証する。
 */
void test_connection_logs_do_not_contain_passwords(void) {
  FakeWiFiState::setDirectResult("primary", WL_CONNECT_FAILED);
  FakeWiFiState::setMultiResult(WL_CONNECT_FAILED);
  WiFiESP32 wifi("primary", "primary-secret");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-secret"));

  TEST_ASSERT_FALSE(wifi.begin());
  for (const std::string& message : FakeLogState::messages) {
    TEST_ASSERT_EQUAL_INT(-1, static_cast<int>(message.find("primary-secret")));
    TEST_ASSERT_EQUAL_INT(-1,
                          static_cast<int>(message.find("fallback-secret")));
  }
}

/**
 * @brief 直前の失敗で残ったステータスに影響されず次候補へ接続することを検証する。
 *
 * ESP32 Arduino Coreのステータスはイベントでしか更新されず、
 * WL_NO_SSID_AVAILが残ったままになる。この値を接続失敗と誤判定すると
 * フォールバック候補へ一度も接続できなくなる。
 */
void test_connect_succeeds_when_previous_attempt_left_stale_status(void) {
  FakeWiFiState::setDirectResult("primary", WL_NO_SSID_AVAIL, 100);
  FakeWiFiState::setDirectResult("fallback", WL_CONNECTED, 800);
  FakeWiFiState::addScanNetwork(
      "fallback", -40, 6, {{0x40, 0x41, 0x42, 0x43, 0x44, 0x45}});
  WiFiESP32 wifi("primary", "primary-password");
  TEST_ASSERT_TRUE(wifi.addAP("fallback", "fallback-password",
                              "192.168.4.50", "192.168.4.1",
                              "255.255.255.0"));

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_STRING("fallback", wifi.getConnectedSsid().c_str());
  TEST_ASSERT_EQUAL_UINT32(2, FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("fallback",
                           FakeWiFiState::beginCalls[1].ssid.c_str());
}

/**
 * @brief 接続確立に時間がかかってもタイムアウト内なら成功と判定することを検証する。
 */
void test_connect_waits_until_connection_is_established(void) {
  // WIFI_PRIMARY_CONNECT_WAIT(3000ms)内に確立するケース
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED, 2000);
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_TRUE(wifi.begin());
  TEST_ASSERT_EQUAL_STRING("primary", wifi.getConnectedSsid().c_str());
}

/**
 * @brief タイムアウトを超えて確立しない場合は失敗と判定することを検証する。
 */
void test_connect_gives_up_when_timeout_elapses(void) {
  // WIFI_PRIMARY_CONNECT_WAIT(3000ms)を超えて確立するケース
  FakeWiFiState::setDirectResult("primary", WL_CONNECTED, 3100);
  WiFiESP32 wifi("primary", "primary-password");

  TEST_ASSERT_FALSE(wifi.begin());
  TEST_ASSERT_FALSE(wifi.isConnected());
}

/**
 * @brief 未接続時のdisconnectがステータスを変えないことを検証する。
 *
 * 実機のesp_wifi_disconnect()は接続中でなければイベントを発生させない。
 * モックがこの挙動を再現していないと、古いステータスの問題を検出できない。
 */
void test_disconnect_does_not_reset_status_when_not_connected(void) {
  WiFi.mode(WIFI_STA);
  FakeWiFiState::setDirectResult("primary", WL_NO_SSID_AVAIL, 100);

  WiFi.begin("primary", "primary-password");
  delay(100);
  TEST_ASSERT_EQUAL_INT(WL_NO_SSID_AVAIL, WiFi.status());

  WiFi.disconnect(false, false);
  TEST_ASSERT_EQUAL_INT(WL_NO_SSID_AVAIL, WiFi.status());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_legacy_constructor_uses_primary_credentials);
  RUN_TEST(test_wifi_esp32_is_not_copyable);
  RUN_TEST(test_add_ap_registers_valid_fallback);
  RUN_TEST(test_add_ap_registers_static_network_profile);
  RUN_TEST(test_add_ap_rejects_invalid_static_network_profile);
  RUN_TEST(test_add_ap_rejects_empty_ssid);
  RUN_TEST(test_add_ap_rejects_32_character_ssid);
  RUN_TEST(test_add_ap_rejects_password_longer_than_64_characters);
  RUN_TEST(test_add_ap_rejects_duplicate_primary_ssid);
  RUN_TEST(test_add_ap_rejects_duplicate_fallback_ssid);
  RUN_TEST(test_deep_sleep_uses_last_successful_ap_fast_path);
  RUN_TEST(test_deep_sleep_ignores_rtc_ssid_not_in_current_candidates);
  RUN_TEST(test_failed_fast_path_falls_back_to_primary);
  RUN_TEST(test_primary_success_skips_wifi_multi);
  RUN_TEST(test_primary_failure_enables_dhcp_and_runs_wifi_multi);
  RUN_TEST(test_static_fallback_scans_once_and_tries_visible_profiles_by_rssi);
  RUN_TEST(test_static_fallback_uses_strongest_bssid_for_duplicate_ssid);
  RUN_TEST(test_deep_sleep_applies_static_network_for_last_fallback);
  RUN_TEST(test_get_connected_ssid_returns_current_wifi_ssid);
  RUN_TEST(test_fallback_success_is_saved_for_next_deep_sleep);
  RUN_TEST(test_all_candidates_failure_returns_false);
  RUN_TEST(test_static_ip_is_applied_to_primary_connection);
  RUN_TEST(test_static_ip_rejects_invalid_address_strings);
  RUN_TEST(test_static_ip_rejects_null_before_parsing);
  RUN_TEST(test_health_check_backs_off_for_ten_seconds_after_failure);
  RUN_TEST(test_health_check_retries_after_ten_seconds);
  RUN_TEST(test_health_check_returns_immediately_when_connected);
  RUN_TEST(test_connection_logs_do_not_contain_passwords);
  RUN_TEST(test_connect_succeeds_when_previous_attempt_left_stale_status);
  RUN_TEST(test_connect_waits_until_connection_is_established);
  RUN_TEST(test_connect_gives_up_when_timeout_elapses);
  RUN_TEST(test_disconnect_does_not_reset_status_when_not_connected);
  return UNITY_END();
}
