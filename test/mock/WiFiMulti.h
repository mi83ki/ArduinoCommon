#pragma once

#include <cstdint>

#include "WiFi.h"

class WiFiMulti {
 public:
 bool addAP(const char* ssid, const char* password = nullptr);
  uint8_t run(uint32_t connectTimeout = 5000);
};
