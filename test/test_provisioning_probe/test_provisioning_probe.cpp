#include <unity.h>
#include "WiFi.h"
#include "esp_netif.h"
#include "provisioning/WiFiProvisioningProbe.h"

using namespace ArduinoCommon;
void setUp() {FakeWiFiState::reset();}
void tearDown() {}
WiFiProfile profile() {WiFiProfile p;p.id=2;p.ssid="target";p.password="password";return p;}
ApCredentials credentials{"Example-A1B2C3","ABCDEFGHIJKLMNOPQRST"};
/** @brief 指定した1プロファイルをコピーして試験し、期限内のSSIDとIP一致だけを採用する。 */
void test_probe_connects_only_requested_profile_without_waiting() {
  WiFiProvisioningProbe probe;TEST_ASSERT_TRUE(probe.begin(credentials));
  TEST_ASSERT_EQUAL(WIFI_AP_STA,FakeProvisioning::state().mode);
  TEST_ASSERT_FALSE(FakeProvisioning::state().persistent);TEST_ASSERT_FALSE(FakeProvisioning::state().autoReconnect);
  TEST_ASSERT_EQUAL(1,FakeProvisioning::state().clients);
  FakeWiFiState::setDirectResult("target",WL_CONNECTED,100);
  FakeNetif::servers()={{0x09090909,0x01010101,0x08080808}};
  auto p=profile();const auto before=millis();TEST_ASSERT_TRUE(probe.start(p,41,before+20000));p.ssid="changed";
  TEST_ASSERT_EQUAL(before,millis());TEST_ASSERT_EQUAL(1,FakeWiFiState::beginCalls.size());
  TEST_ASSERT_EQUAL_STRING("target",FakeWiFiState::beginCalls[0].ssid.c_str());
  TEST_ASSERT_TRUE(FakeWiFiState::configCalls.back().ip==IPAddress());
  for(auto value:FakeNetif::servers())TEST_ASSERT_EQUAL(0,value);
  TEST_ASSERT_FALSE(FakeNetif::unsafeDnsCall());
  TEST_ASSERT_FALSE(probe.start(profile(),42,millis()+20000));TEST_ASSERT_FALSE(probe.startScan());
  probe.poll();TEST_ASSERT_EQUAL(WiFiProbeState::Connecting,probe.result().state);
  fakeMillis+=100;probe.poll();TEST_ASSERT_EQUAL(WiFiProbeState::Succeeded,probe.result().state);
  TEST_ASSERT_EQUAL(41,probe.result().jobId);TEST_ASSERT_EQUAL(2,probe.result().profileId);
  TEST_ASSERT_TRUE(probe.finish(41));TEST_ASSERT_TRUE(FakeProvisioning::state().ap);
}
/** @brief 取消・時間切れ後はAPへ戻り、違うジョブや遅い接続結果を採用しない。 */
void test_probe_timeout_cancel_and_late_result() {
  WiFiProvisioningProbe probe;TEST_ASSERT_TRUE(probe.begin(credentials));
  FakeWiFiState::setDirectResult("target",WL_CONNECTED,21000);
  TEST_ASSERT_TRUE(probe.start(profile(),1,millis()+20000));TEST_ASSERT_FALSE(probe.cancel(2));
  fakeMillis+=20000;probe.poll();TEST_ASSERT_EQUAL(WiFiProbeState::Failed,probe.result().state);
  TEST_ASSERT_TRUE(FakeProvisioning::state().ap);
  fakeMillis+=2000;probe.poll();TEST_ASSERT_EQUAL(WiFiProbeState::Failed,probe.result().state);
  TEST_ASSERT_TRUE(probe.start(profile(),2,millis()+20000));TEST_ASSERT_TRUE(probe.cancel(2));
  TEST_ASSERT_EQUAL(WiFiProbeState::Cancelled,probe.result().state);
  fakeMillis+=22000;probe.poll();TEST_ASSERT_EQUAL(WiFiProbeState::Cancelled,probe.result().state);
  TEST_ASSERT_TRUE(FakeProvisioning::state().ap);
}
/** @brief APと重なる固定網の試験ではAPを停止し、失敗後に同じ資格情報で復帰する。 */
void test_probe_static_dns_and_overlapping_ap() {
  WiFiProvisioningProbe probe;TEST_ASSERT_TRUE(probe.begin(credentials));
  auto p=profile();p.staticIp=true;p.ip={{192,168,4,10}};p.gateway={{192,168,4,254}};
  p.mask={{255,255,255,0}};p.dns1={{1,1,1,1}};
  FakeNetif::servers()={{0x09090909,0x08080808,0x04040404}};
  TEST_ASSERT_TRUE(probe.start(p,1,millis()+20000));
  TEST_ASSERT_FALSE(FakeProvisioning::state().ap);TEST_ASSERT_TRUE(probe.result().apSuspended);
  TEST_ASSERT_TRUE(FakeWiFiState::configCalls.back().dns1==IPAddress(1,1,1,1));
  TEST_ASSERT_EQUAL_HEX32(0x01010101,FakeNetif::servers()[0]);
  TEST_ASSERT_EQUAL(0,FakeNetif::servers()[1]);TEST_ASSERT_EQUAL(0,FakeNetif::servers()[2]);
  TEST_ASSERT_FALSE(FakeNetif::unsafeDnsCall());
  TEST_ASSERT_TRUE(probe.cancel(1));TEST_ASSERT_TRUE(FakeProvisioning::state().ap);
  TEST_ASSERT_EQUAL_STRING(credentials.password.c_str(),FakeProvisioning::state().apPassword.c_str());
}
/** @brief 非同期スキャンを直列化し、強い順に重複を除いて最大20件だけ返す。 */
void test_scan_is_bounded_cached_and_serialized() {
  for(int i=0;i<25;++i)FakeWiFiState::addScanNetwork(("wifi"+std::to_string(i)).c_str(),-90+i,1,{{0,1,2,3,4,5}});
  FakeWiFiState::addScanNetwork("wifi24",-10,6,{{0,1,2,3,4,6}});
  WiFiProvisioningProbe probe;TEST_ASSERT_TRUE(probe.begin(credentials));
  TEST_ASSERT_TRUE(probe.startScan());TEST_ASSERT_FALSE(probe.start(profile(),1,millis()+20000));
  FakeProvisioning::state().scanResult=26;probe.poll();
  TEST_ASSERT_EQUAL(WiFiScanState::Ready,probe.scanState());
  const auto results=probe.scanResults();TEST_ASSERT_EQUAL(20,results.size());
  TEST_ASSERT_EQUAL_STRING("wifi24",results[0].ssid.c_str());TEST_ASSERT_EQUAL(-10,results[0].rssi);
  const auto scans=FakeWiFiState::scanCalls;TEST_ASSERT_TRUE(probe.startScan());TEST_ASSERT_EQUAL(scans,FakeWiFiState::scanCalls);
  TEST_ASSERT_GREATER_THAN(0,FakeWiFiState::scanDeleteCalls);
  fakeMillis+=30001;TEST_ASSERT_TRUE(probe.startScan());probe.stop();
  TEST_ASSERT_FALSE(FakeProvisioning::state().ap);TEST_ASSERT_GREATER_THAN(0,FakeProvisioning::state().scanStops);
}
/** @brief DNS解除失敗時は接続を開始せず、設定用APへ復帰する。 */
void test_probe_dns_clear_failure_restores_ap() {
  WiFiProvisioningProbe probe;TEST_ASSERT_TRUE(probe.begin(credentials));
  FakeNetif::execFails()=true;
  TEST_ASSERT_TRUE(probe.start(profile(),1,millis()+20000));
  TEST_ASSERT_EQUAL(WiFiProbeState::Failed,probe.result().state);
  TEST_ASSERT_EQUAL_STRING("network_config_failed",probe.result().error.c_str());
  TEST_ASSERT_EQUAL(0,FakeWiFiState::beginCalls.size());TEST_ASSERT_TRUE(probe.apAvailable());
}
int main() {UNITY_BEGIN();RUN_TEST(test_probe_connects_only_requested_profile_without_waiting);
  RUN_TEST(test_probe_dns_clear_failure_restores_ap);
  RUN_TEST(test_probe_timeout_cancel_and_late_result);RUN_TEST(test_probe_static_dns_and_overlapping_ap);
  RUN_TEST(test_scan_is_bounded_cached_and_serialized);return UNITY_END();}
