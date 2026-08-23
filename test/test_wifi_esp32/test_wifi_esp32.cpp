#include <unity.h>

#include <array>
#include <cstring>

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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_legacy_constructor_uses_primary_credentials);
  RUN_TEST(test_add_ap_registers_valid_fallback);
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
  RUN_TEST(test_fallback_success_is_saved_for_next_deep_sleep);
  RUN_TEST(test_all_candidates_failure_returns_false);
  RUN_TEST(test_static_ip_is_applied_to_primary_connection);
  RUN_TEST(test_static_ip_rejects_invalid_address_strings);
  RUN_TEST(test_static_ip_rejects_null_before_parsing);
  RUN_TEST(test_health_check_backs_off_for_ten_seconds_after_failure);
  RUN_TEST(test_health_check_retries_after_ten_seconds);
  RUN_TEST(test_health_check_returns_immediately_when_connected);
  RUN_TEST(test_connection_logs_do_not_contain_passwords);
  return UNITY_END();
}
