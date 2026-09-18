#pragma once
#include "WiFi.h"
#include "esp_netif.h"
enum wifi_scan_type_t {WIFI_SCAN_TYPE_ACTIVE, WIFI_SCAN_TYPE_PASSIVE};
struct wifi_scan_config_t {
  uint8_t* ssid=nullptr;uint8_t* bssid=nullptr;uint8_t channel=0;bool show_hidden=false;
  wifi_scan_type_t scan_type=WIFI_SCAN_TYPE_ACTIVE;
  struct {struct {uint32_t min=0,max=0;} active;uint32_t passive=0;} scan_time;
  uint8_t home_chan_dwell_time=0;
};
namespace FakeScan {
inline wifi_scan_config_t& config(){static wifi_scan_config_t value;return value;}
inline bool& reject(){static bool value=false;return value;}
inline bool& blocking(){static bool value=false;return value;}
}
inline esp_err_t esp_wifi_scan_start(const wifi_scan_config_t* config,bool block) {
  FakeScan::config()=*config;FakeScan::blocking()=block;
  if(FakeScan::reject())return -1;
  ++FakeWiFiState::scanCalls;FakeProvisioning::state().scanStarted=millis();
  FakeProvisioning::state().scanTimeout=10000;return ESP_OK;
}
inline esp_err_t esp_wifi_scan_stop() {++FakeProvisioning::state().scanStops;return ESP_OK;}
