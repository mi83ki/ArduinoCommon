/**
 * @file test_provisioning_identity.cpp
 * @brief AP認証情報の初回生成・読込・破損時動作を検証するテスト。
 */

#include <unity.h>
#include "MemorySettingsBackend.h"
#include "provisioning/ApCredentialStore.h"
#include <algorithm>
#include <cstring>

using namespace ArduinoCommon;
#define STATUS(expected,expression) TEST_ASSERT_EQUAL_INT(int(SettingsStatus::expected),int(expression))
void setUp() {}
void tearDown() {}
const std::array<uint8_t,6> mac{{0,1,2,0xa1,0xb2,0xc3}};
bool randomBytes(uint8_t* bytes,size_t length) {for(size_t i=0;i<length;++i)bytes[i]=uint8_t(i);return true;}

/** @brief 初回だけ生成して保存し、次回は乱数を使わず同じラベル情報を取得する。 */
void test_identity_is_created_once_and_read_back() {
  MemorySettingsBackend backend;int randomCalls=0;
  ApCredentialStore store(backend,"BumbleEye-",mac,[&](uint8_t* p,size_t n){++randomCalls;return randomBytes(p,n);});
  ApCredentials credentials;STATUS(Ok,store.loadOrCreate(credentials));
  TEST_ASSERT_EQUAL_STRING("BumbleEye-A1B2C3",credentials.ssid.c_str());
  TEST_ASSERT_EQUAL(20,credentials.password.size());
  for(char c:credentials.password)TEST_ASSERT_NOT_NULL(std::strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567",c));
  TEST_ASSERT_EQUAL(1,backend.writes);TEST_ASSERT_EQUAL(1,randomCalls);
  ApCredentialStore restarted(backend,"ignored-",mac,{});ApCredentials loaded;
  STATUS(Ok,restarted.loadOrCreate(loaded));TEST_ASSERT_EQUAL_STRING(credentials.password.c_str(),loaded.password.c_str());
  TEST_ASSERT_EQUAL_STRING(credentials.ssid.c_str(),loaded.ssid.c_str());TEST_ASSERT_EQUAL(1,backend.writes);
}
/** @brief 書込み・読戻しに失敗した資格情報を公開せず、破損時は自動生成しない。 */
void test_identity_failure_and_corruption_do_not_replace_labels() {
  for(bool readback:{false,true}) {
    MemorySettingsBackend backend;backend.failWrite=1;backend.failReadback=readback;
    ApCredentialStore store(backend,"Test-",mac,randomBytes);ApCredentials output{"old","secret"};
    TEST_ASSERT_NOT_EQUAL(int(SettingsStatus::Ok),int(store.loadOrCreate(output)));
    TEST_ASSERT_EQUAL_STRING("old",output.ssid.c_str());
  }
  MemorySettingsBackend backend;backend.values["ap_auth"]={1,2,3};
  ApCredentialStore store(backend,"Test-",mac,randomBytes);ApCredentials output;
  STATUS(Corrupt,store.loadOrCreate(output));TEST_ASSERT_EQUAL(0,backend.writes);
  STATUS(Ok,store.regenerate(output));TEST_ASSERT_EQUAL(1,backend.writes);
}
/** @brief 乱数失敗と長すぎる接頭辞は保存せず、QR予約文字をエスケープする。 */
void test_identity_validates_generation_and_escapes_qr() {
  MemorySettingsBackend backend;ApCredentials output;
  ApCredentialStore unavailable(backend,"Test-",mac,[](uint8_t*,size_t){return false;});
  STATUS(IoError,unavailable.loadOrCreate(output));
  ApCredentialStore tooLong(backend,std::string(26,'x'),mac,randomBytes);
  STATUS(InvalidArgument,tooLong.loadOrCreate(output));TEST_ASSERT_EQUAL(0,backend.writes);
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b\\:c\\,d\\\"e\\\\f;P:pass\\;word;;",
      ApCredentialStore::wifiQr({"a;b:c,d\"e\\f","pass;word"}).c_str());
}
int main() {UNITY_BEGIN();RUN_TEST(test_identity_is_created_once_and_read_back);
  RUN_TEST(test_identity_failure_and_corruption_do_not_replace_labels);
  RUN_TEST(test_identity_validates_generation_and_escapes_qr);return UNITY_END();}
