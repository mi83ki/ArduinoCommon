#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Arduino.h"
#include "esp_system.h"

typedef enum {
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL = 1,
  WL_SCAN_COMPLETED = 2,
  WL_CONNECTED = 3,
  WL_CONNECT_FAILED = 4,
  WL_CONNECTION_LOST = 5,
  WL_DISCONNECTED = 6,
} wl_status_t;

static const uint8_t WIFI_STA = 1;
static const uint32_t INADDR_NONE = 0;

class IPAddress {
 public:
  IPAddress();
  explicit IPAddress(uint32_t value);
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth);

  bool fromString(const char* value);
  String toString() const;
  bool operator==(const IPAddress& value) const;
  bool operator!=(const IPAddress& value) const;
  uint8_t operator[](size_t index) const;

 private:
  std::array<uint8_t, 4> _bytes;
};

namespace FakeWiFiState {

struct BeginCall {
  std::string ssid;
  std::string password;
  int32_t channel;
  std::array<uint8_t, 6> bssid;
  bool hasBssid;
};

struct ConfigCall {
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
};

struct ConnectionResult {
  wl_status_t status;
  uint32_t durationMs;
  std::string ssid;
  std::array<uint8_t, 6> bssid;
  int32_t channel;
  IPAddress ip;
  int32_t rssi;
};

extern std::vector<BeginCall> beginCalls;
extern std::vector<ConfigCall> configCalls;
extern std::vector<std::pair<std::string, std::string>> addedAps;
extern uint32_t modeCalls;
extern uint32_t disconnectCalls;
extern uint32_t waitCalls;
extern uint32_t multiRunCalls;
extern uint32_t nullAddressParseCalls;
extern esp_reset_reason_t resetReason;

void reset();
void setDirectResult(const char* ssid, wl_status_t status,
                     uint32_t durationMs = 100);
void setMultiResult(wl_status_t status, const char* ssid = "",
                    uint32_t durationMs = 100, int32_t channel = 6,
                    const std::array<uint8_t, 6>& bssid =
                        {{0x10, 0x20, 0x30, 0x40, 0x50, 0x60}});
void applyMultiResult(uint32_t timeoutMs);
const ConnectionResult& multiResult();

}  // namespace FakeWiFiState

class FakeWiFiClass {
 public:
  void mode(uint8_t mode);
  wl_status_t begin(const char* ssid, const char* password = nullptr,
                    int32_t channel = 0, const uint8_t* bssid = nullptr,
                    bool connect = true);
  wl_status_t waitForConnectResult(uint32_t timeoutMs = 60000);
  bool config(IPAddress ip, IPAddress gateway, IPAddress subnet,
              IPAddress dns1 = IPAddress(), IPAddress dns2 = IPAddress());
  bool disconnect(bool wifiOff = false, bool eraseAp = false);
  wl_status_t status() const;
  String SSID() const;
  IPAddress localIP() const;
  int32_t RSSI() const;
  uint8_t* BSSID();
  int32_t channel() const;

  void applyConnectionResult(const FakeWiFiState::ConnectionResult& result,
                             uint32_t timeoutMs);

 private:
  wl_status_t _status = WL_DISCONNECTED;
  FakeWiFiState::ConnectionResult _pendingResult{
      WL_CONNECT_FAILED, 0, "", {{0, 0, 0, 0, 0, 0}}, 0, IPAddress(), -100};
  String _ssid;
  IPAddress _ip;
  int32_t _rssi = -100;
  std::array<uint8_t, 6> _bssid{{0, 0, 0, 0, 0, 0}};
  int32_t _channel = 0;
};

extern FakeWiFiClass WiFi;
