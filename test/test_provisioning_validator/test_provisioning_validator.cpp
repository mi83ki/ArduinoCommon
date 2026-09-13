#include <unity.h>
#include "provisioning/WiFiProfileValidator.h"

using namespace ArduinoCommon;
void setUp() {}
void tearDown() {}
WiFiProfile profile() {WiFiProfile p;p.ssid="日本語ネットワーク";p.password="password";return p;}
/** @brief SDKに依存せずSSID・PSK・最大件数・主IDの存在と重複を検証する。 */
void test_profiles_validate_text_ids_and_limits() {
  auto p=profile();TEST_ASSERT_TRUE(WiFiProfileValidator::validate({p},0,true).empty());
  TEST_ASSERT_TRUE(WiFiProfileValidator::validate({},0,false).empty());
  TEST_ASSERT_FALSE(WiFiProfileValidator::validate({},0,true).empty());
  TEST_ASSERT_FALSE(WiFiProfileValidator::validate({p},1,true).empty());
  TEST_ASSERT_FALSE(WiFiProfileValidator::validate({p,p},0,true).empty());
  p.ssid=std::string(32,'x');TEST_ASSERT_FALSE(WiFiProfileValidator::validate({p},0,true).empty());
  p=profile();p.ssid=std::string("a\0b",3);TEST_ASSERT_FALSE(WiFiProfileValidator::validate({p},0,true).empty());
  TEST_ASSERT_FALSE(WiFiProfileValidator::validUtf8("\xc0\xaf"));
  TEST_ASSERT_TRUE(WiFiProfileValidator::validPassword(std::string(64,'a')));
  TEST_ASSERT_FALSE(WiFiProfileValidator::validPassword(std::string(64,'g')));
  TEST_ASSERT_FALSE(WiFiProfileValidator::validPassword("short"));
}
/** @brief 固定IPのネットワーク・GW・DNSを検証し、DHCP候補を妨げない。 */
void test_static_network_checks_subnet_and_dns() {
  auto p=profile();p.staticIp=true;p.ip={{192,168,1,10}};p.gateway={{192,168,1,1}};p.mask={{255,255,255,0}};
  TEST_ASSERT_TRUE(WiFiProfileValidator::validStaticIp(p,false));
  TEST_ASSERT_FALSE(WiFiProfileValidator::validStaticIp(p,true));
  p.dns1={{192,168,1,1}};TEST_ASSERT_TRUE(WiFiProfileValidator::validStaticIp(p,true));
  p.ip={{192,168,1,0}};TEST_ASSERT_FALSE(WiFiProfileValidator::validStaticIp(p,false));
  p.ip={{192,168,1,10}};p.mask={{255,0,255,0}};TEST_ASSERT_FALSE(WiFiProfileValidator::validStaticIp(p,false));
  p.mask={{255,255,255,0}};p.gateway={{192,168,2,1}};TEST_ASSERT_FALSE(WiFiProfileValidator::validStaticIp(p,false));
}
/** @brief keepは同一ID・SSIDだけに限定し、認証変更は明示的な操作を要求する。 */
void test_password_actions_preserve_only_matching_profile() {
  auto saved=profile(),candidate=saved;candidate.password="";
  TEST_ASSERT_TRUE(WiFiProfileValidator::applyPassword(candidate,WiFiPasswordAction::Keep,"",false,{saved}));
  TEST_ASSERT_EQUAL_STRING("password",candidate.password.c_str());candidate.ssid="different";
  TEST_ASSERT_FALSE(WiFiProfileValidator::applyPassword(candidate,WiFiPasswordAction::Keep,"",false,{saved}));
  TEST_ASSERT_EQUAL_STRING("password",candidate.password.c_str());
  TEST_ASSERT_FALSE(WiFiProfileValidator::applyPassword(candidate,WiFiPasswordAction::Clear,"",false,{saved}));
  TEST_ASSERT_TRUE(WiFiProfileValidator::applyPassword(candidate,WiFiPasswordAction::Clear,"",true,{saved}));
  TEST_ASSERT_TRUE(candidate.password.empty());
  TEST_ASSERT_TRUE(WiFiProfileValidator::applyPassword(candidate,WiFiPasswordAction::Replace,"new password",false,{saved}));
  TEST_ASSERT_EQUAL_STRING("new password",candidate.password.c_str());
}
int main() {UNITY_BEGIN();RUN_TEST(test_profiles_validate_text_ids_and_limits);
  RUN_TEST(test_static_network_checks_subnet_and_dns);RUN_TEST(test_password_actions_preserve_only_matching_profile);
  return UNITY_END();}
