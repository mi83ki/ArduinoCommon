#pragma once
#include "WiFi.h"
namespace FakeDns {inline bool& running() {static bool value=false;return value;}}
class DNSServer {
 public:
  bool start(const uint16_t&,const String&,const IPAddress&) {FakeDns::running()=true;return true;}
  void stop() {FakeDns::running()=false;}
  void processNextRequest() {}
};
