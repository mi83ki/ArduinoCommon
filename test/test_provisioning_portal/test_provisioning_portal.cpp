#include <unity.h>
#include "provisioning/ProvisioningPortalESP32.h"
#include "lwip/sockets.h"

using namespace ArduinoCommon;
void setUp() {FakeWiFiState::reset();FakeSockets::destinations()[1]="192.168.4.1";}
void tearDown() {}
ApCredentials credentials{"Example-A1B2C3","ABCDEFGHIJKLMNOPQRST"};
bool randomBytes(uint8_t* bytes,size_t length) {std::memset(bytes,0xab,length);return true;}
httpd_req_t request(const char* path,httpd_method_t method=HTTP_GET) {
  httpd_req_t r;r.uri=path;r.method=method;r.headers["Host"]="192.168.4.1";
  r.headers["X-Setup-Token"]=std::string(32,'a');
  r.headers["X-Setup-Token"]="abababababababababababababababab";
  if(method==HTTP_POST) {r.body="{}";r.content_len=2;r.headers["Content-Type"]="application/json";}
  return r;
}
/** @brief AP宛てのsessionだけがtokenを取得でき、全登録APIへ同じ検証を適用する。 */
void test_portal_enforces_ap_host_origin_and_session() {
  WiFiProvisioningProbe probe;ProvisioningPortalESP32 portal(probe);int calls=0;
  TEST_ASSERT_TRUE(portal.addHandler(PortalMethod::Post,"/api/custom",[&](const PortalRequest& r){++calls;return PortalResponse{200,r.body};}));
  TEST_ASSERT_TRUE(portal.begin(credentials,randomBytes));
  TEST_ASSERT_EQUAL(2,FakeHttp::state().config.max_open_sockets);TEST_ASSERT_EQUAL(24,FakeHttp::state().config.max_uri_handlers);
  auto session=request("/api/session");session.headers.erase("X-Setup-Token");FakeHttp::request(session);
  TEST_ASSERT_EQUAL_STRING("200 OK",session.status.c_str());TEST_ASSERT_TRUE(session.response.find("abababababababababababababababab")!=std::string::npos);
  TEST_ASSERT_TRUE(session.response.find(credentials.password)==std::string::npos);
  for(int mode=0;mode<4;++mode) {
    auto r=request("/api/custom",HTTP_POST);
    if(mode==0)r.headers["Host"]="attacker.example";
    if(mode==1)r.headers["Origin"]="https://attacker.example";
    if(mode==2)r.headers["X-Setup-Token"]="wrong";
    if(mode==3)FakeSockets::destinations()[1]="192.168.1.50";
    FakeHttp::request(r);TEST_ASSERT_EQUAL_STRING("403 Forbidden",r.status.c_str());TEST_ASSERT_EQUAL(0,r.recvCalls);
    FakeSockets::destinations()[1]="192.168.4.1";
  }
  TEST_ASSERT_EQUAL(0,calls);auto good=request("/api/custom",HTTP_POST);FakeHttp::request(good);TEST_ASSERT_EQUAL(1,calls);
  TEST_ASSERT_EQUAL_STRING("no-store",good.responseHeaders["Cache-Control"].c_str());
  FakeSockets::destinations()[1]="192.168.1.50";
  TEST_ASSERT_NOT_EQUAL(ESP_OK,FakeHttp::state().config.open_fn(&FakeHttp::state(),1));
}
/** @brief body上限は読込前に拒否し、総受信期限と途中切断では製品処理を実行しない。 */
void test_portal_body_limits_and_deadlines() {
  WiFiProvisioningProbe probe;ProvisioningPortalESP32 portal(probe);int calls=0;
  portal.addHandler(PortalMethod::Post,"/api/custom",[&](const PortalRequest&){++calls;return PortalResponse{};});
  TEST_ASSERT_TRUE(portal.begin(credentials,randomBytes));
  auto large=request("/api/custom",HTTP_POST);large.content_len=4097;FakeHttp::request(large);
  TEST_ASSERT_EQUAL_STRING("413 Payload Too Large",large.status.c_str());TEST_ASSERT_EQUAL(0,large.recvCalls);
  auto slow=request("/api/custom",HTTP_POST);slow.body=std::string(4,'x');slow.content_len=4;slow.chunk=1;slow.recvDelay=1100;
  FakeHttp::request(slow);TEST_ASSERT_EQUAL_STRING("408 Request Timeout",slow.status.c_str());
  auto broken=request("/api/custom",HTTP_POST);broken.disconnect=true;FakeHttp::request(broken);
  TEST_ASSERT_EQUAL_STRING("400 Bad Request",broken.status.c_str());
  auto wrong=request("/api/custom",HTTP_POST);wrong.headers["Content-Type"]="text/plain";FakeHttp::request(wrong);
  TEST_ASSERT_EQUAL_STRING("415 Unsupported Media Type",wrong.status.c_str());TEST_ASSERT_EQUAL(0,calls);
}
/** @brief CNAは案内だけ、gzipは指定長で配信し、HTTP中の停止要求を所有タスクへ渡す。 */
void test_portal_cna_asset_and_deferred_stop() {
  WiFiProvisioningProbe probe;ProvisioningPortalESP32 portal(probe);
  const uint8_t asset[]={31,139,0,1};
  portal.addHandler(PortalMethod::Post,"/api/stop",[&](const PortalRequest&){portal.requestStop();return PortalResponse{202,"{}"};});
  TEST_ASSERT_TRUE(portal.begin(credentials,randomBytes,asset,sizeof(asset)));
  auto cna=request("/hotspot-detect.html");cna.headers["Host"]="captive.apple.com";FakeHttp::request(cna);
  TEST_ASSERT_TRUE(cna.response.find("http://192.168.4.1")!=std::string::npos);
  TEST_ASSERT_TRUE(cna.response.find("Safari")!=std::string::npos);TEST_ASSERT_TRUE(cna.response.find("abababab")==std::string::npos);
  auto root=request("/");FakeHttp::request(root);TEST_ASSERT_EQUAL(4,root.response.size());
  TEST_ASSERT_EQUAL_MEMORY(asset,root.response.data(),4);TEST_ASSERT_EQUAL_STRING("gzip",root.responseHeaders["Content-Encoding"].c_str());
  auto stop=request("/api/stop",HTTP_POST);FakeHttp::request(stop);TEST_ASSERT_TRUE(portal.running());
  TEST_ASSERT_EQUAL(0,FakeHttp::state().stops);portal.tick();TEST_ASSERT_FALSE(portal.running());
  TEST_ASSERT_FALSE(FakeDns::running());TEST_ASSERT_FALSE(FakeProvisioning::state().ap);
}
/** @brief スキャンはHTTPから開始せず、結果JSONのSSID特殊文字を正しくエスケープする。 */
void test_portal_scan_is_queued_and_json_escaped() {
  FakeWiFiState::addScanNetwork("quote\"slash\\",-50,1,{{0,1,2,3,4,5}});
  WiFiProvisioningProbe probe;ProvisioningPortalESP32 portal(probe);TEST_ASSERT_TRUE(portal.begin(credentials,randomBytes));
  auto scan=request("/api/scan",HTTP_POST);FakeHttp::request(scan);TEST_ASSERT_EQUAL_STRING("202 Accepted",scan.status.c_str());
  TEST_ASSERT_EQUAL(0,FakeWiFiState::scanCalls);portal.tick();TEST_ASSERT_EQUAL(1,FakeWiFiState::scanCalls);
  FakeProvisioning::state().scanResult=1;portal.tick();
  auto result=request("/api/scan");FakeHttp::request(result);
  TEST_ASSERT_TRUE(result.response.find("quote\\\"slash\\\\")!=std::string::npos);
  fakeMillis+=1000;const auto activity=portal.lastActivityMillis();FakeHttp::request(result);
  TEST_ASSERT_EQUAL(activity,portal.lastActivityMillis());
}
int main() {UNITY_BEGIN();RUN_TEST(test_portal_enforces_ap_host_origin_and_session);
  RUN_TEST(test_portal_body_limits_and_deadlines);RUN_TEST(test_portal_cna_asset_and_deferred_stop);
  RUN_TEST(test_portal_scan_is_queued_and_json_escaped);return UNITY_END();}
