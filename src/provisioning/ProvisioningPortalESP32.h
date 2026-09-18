#pragma once
#include "PortalTypes.h"
#if !defined(ARDUINOCOMMON_DISABLE_PROVISIONING) && (defined(ARDUINO_ARCH_ESP32) || defined(ARDUINOCOMMON_TEST_ESP32))
#include "WiFiProvisioningProbe.h"
#include <DNSServer.h>
#include <esp_http_server.h>
#include <atomic>
#include <mutex>

namespace ArduinoCommon {
struct PortalOptions {
  size_t maximumBody=4096;
  uint32_t receiveMillis=3000;
  uint16_t maximumSockets=2,maximumHandlers=24;
  IPv4 apAddress{{192,168,4,1}};
};
class ProvisioningPortalESP32 {
 public:
  using Handler=std::function<PortalResponse(const PortalRequest&)>;
  explicit ProvisioningPortalESP32(WiFiProvisioningProbe&,PortalOptions options={});
  ~ProvisioningPortalESP32();
  bool addHandler(PortalMethod,const std::string& path,Handler);
  bool setSessionHandler(std::function<PortalResponse(const std::string& token)>);
  bool begin(const ApCredentials&,ApCredentialStore::RandomFill,const uint8_t* gzipHtml=nullptr,size_t size=0);
  void tick();
  void requestStop();
  void stop();
  bool running() const;
  uint32_t lastActivityMillis() const;
 private:
  struct Route {PortalMethod method;std::string path;Handler handler;};
  static esp_err_t dispatch(httpd_req_t*);
  static esp_err_t accept(httpd_handle_t,int socket);
  esp_err_t handle(httpd_req_t*);
  bool apSocket(int socket) const;
  WiFiProvisioningProbe& _probe;
  PortalOptions _options;
  DNSServer _dns;
  httpd_handle_t _server=nullptr;
  std::vector<Route> _routes;
  std::function<PortalResponse(const std::string&)> _sessionHandler;
  std::string _host,_token;
  const uint8_t* _html=nullptr;
  size_t _htmlSize=0;
  mutable std::mutex _mutex;
  bool _scanRequested=false,_scanRejected=false;
  WiFiScanState _scanState=WiFiScanState::Idle;
  std::vector<WiFiScanEntry> _scanResults;
  uint32_t _lastActivity=0;
  std::atomic<bool> _running{false},_stopRequested{false};
};
}
#endif
