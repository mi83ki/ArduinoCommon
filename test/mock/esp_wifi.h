#pragma once
#include "WiFi.h"
#include "esp_netif.h"
inline esp_err_t esp_wifi_scan_stop() {++FakeProvisioning::state().scanStops;return ESP_OK;}
