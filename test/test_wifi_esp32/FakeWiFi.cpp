#include "WiFi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "WiFiMulti.h"
#include "esp_system.h"

uint32_t fakeMillis = 0;
FakeWiFiClass WiFi;

namespace {

std::map<std::string, FakeWiFiState::ConnectionResult> directResults;
FakeWiFiState::ConnectionResult configuredMultiResult{
    WL_CONNECT_FAILED, 100, "", {{0, 0, 0, 0, 0, 0}}, 0, IPAddress(), -100};
std::vector<FakeWiFiState::ScanNetwork> configuredScanNetworks;

}  // namespace

namespace FakeWiFiState {

std::vector<BeginCall> beginCalls;
std::vector<ConfigCall> configCalls;
std::vector<std::pair<std::string, std::string>> addedAps;
uint32_t modeCalls = 0;
uint32_t disconnectCalls = 0;
uint32_t waitCalls = 0;
uint32_t multiRunCalls = 0;
uint32_t scanCalls = 0;
uint32_t scanDeleteCalls = 0;
uint32_t nullAddressParseCalls = 0;
esp_reset_reason_t resetReason = ESP_RST_POWERON;

void reset() {
  fakeMillis = 0;
  beginCalls.clear();
  configCalls.clear();
  addedAps.clear();
  modeCalls = 0;
  disconnectCalls = 0;
  waitCalls = 0;
  multiRunCalls = 0;
  scanCalls = 0;
  scanDeleteCalls = 0;
  nullAddressParseCalls = 0;
  resetReason = ESP_RST_POWERON;
  directResults.clear();
  configuredMultiResult =
      {WL_CONNECT_FAILED, 100, "", {{0, 0, 0, 0, 0, 0}}, 0, IPAddress(), -100};
  configuredScanNetworks.clear();
  WiFi = FakeWiFiClass();
}

void setDirectResult(const char* ssid, wl_status_t status, uint32_t durationMs) {
  directResults[ssid == nullptr ? "" : ssid] =
      {status,
       durationMs,
       ssid == nullptr ? "" : ssid,
       {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
       1,
       IPAddress(192, 168, 1, 20),
       -45};
}

void setMultiResult(wl_status_t status, const char* ssid, uint32_t durationMs,
                    int32_t channel,
                    const std::array<uint8_t, 6>& bssid) {
  configuredMultiResult = {status,
                           durationMs,
                           ssid == nullptr ? "" : ssid,
                           bssid,
                           channel,
                           IPAddress(192, 168, 10, 30),
                           -55};
}

void applyMultiResult(uint32_t timeoutMs) {
  WiFi.applyConnectionResult(configuredMultiResult, timeoutMs);
}

const ConnectionResult& multiResult() { return configuredMultiResult; }

void addScanNetwork(const char* ssid, int32_t rssi, int32_t channel,
                    const std::array<uint8_t, 6>& bssid) {
  configuredScanNetworks.push_back(
      {ssid == nullptr ? "" : ssid, rssi, bssid, channel});
}

}  // namespace FakeWiFiState

IPAddress::IPAddress() : _bytes{{0, 0, 0, 0}} {}

IPAddress::IPAddress(uint32_t value)
    : _bytes{{static_cast<uint8_t>(value & 0xff),
              static_cast<uint8_t>((value >> 8) & 0xff),
              static_cast<uint8_t>((value >> 16) & 0xff),
              static_cast<uint8_t>((value >> 24) & 0xff)}} {}

IPAddress::IPAddress(uint8_t first, uint8_t second, uint8_t third,
                     uint8_t fourth)
    : _bytes{{first, second, third, fourth}} {}

bool IPAddress::fromString(const char* value) {
  if (value == nullptr) {
    ++FakeWiFiState::nullAddressParseCalls;
    return false;
  }
  unsigned int bytes[4];
  char trailing;
  if (std::sscanf(value, "%u.%u.%u.%u%c", &bytes[0], &bytes[1], &bytes[2],
                  &bytes[3], &trailing) != 4) {
    return false;
  }
  for (size_t i = 0; i < 4; ++i) {
    if (bytes[i] > 255) return false;
    _bytes[i] = static_cast<uint8_t>(bytes[i]);
  }
  return true;
}

String IPAddress::toString() const {
  char value[16];
  std::snprintf(value, sizeof(value), "%u.%u.%u.%u", _bytes[0], _bytes[1],
                _bytes[2], _bytes[3]);
  return String(value);
}

bool IPAddress::operator==(const IPAddress& value) const {
  return _bytes == value._bytes;
}

bool IPAddress::operator!=(const IPAddress& value) const {
  return !(*this == value);
}

uint8_t IPAddress::operator[](size_t index) const { return _bytes[index]; }

void FakeWiFiClass::mode(uint8_t) { ++FakeWiFiState::modeCalls; }

wl_status_t FakeWiFiClass::begin(const char* ssid, const char* password,
                                 int32_t channel, const uint8_t* bssid,
                                 bool) {
  FakeWiFiState::BeginCall call;
  call.ssid = ssid == nullptr ? "" : ssid;
  call.password = password == nullptr ? "" : password;
  call.channel = channel;
  call.hasBssid = bssid != nullptr;
  call.bssid = {{0, 0, 0, 0, 0, 0}};
  if (bssid != nullptr) {
    std::copy(bssid, bssid + 6, call.bssid.begin());
  }
  FakeWiFiState::beginCalls.push_back(call);

  const auto result = directResults.find(call.ssid);
  _pendingResult =
      result == directResults.end()
          ? FakeWiFiState::ConnectionResult{
                WL_CONNECT_FAILED, 100, call.ssid, call.bssid, channel,
                IPAddress(), -100}
          : result->second;
  if (bssid != nullptr) {
    _pendingResult.bssid = call.bssid;
    _pendingResult.channel = channel;
  }
  _status = WL_IDLE_STATUS;
  return _status;
}

wl_status_t FakeWiFiClass::waitForConnectResult(uint32_t timeoutMs) {
  ++FakeWiFiState::waitCalls;
  applyConnectionResult(_pendingResult, timeoutMs);
  return _status;
}

bool FakeWiFiClass::config(IPAddress ip, IPAddress gateway, IPAddress subnet,
                           IPAddress, IPAddress) {
  FakeWiFiState::configCalls.push_back({ip, gateway, subnet});
  return true;
}

bool FakeWiFiClass::disconnect(bool, bool) {
  ++FakeWiFiState::disconnectCalls;
  _status = WL_DISCONNECTED;
  return true;
}

int16_t FakeWiFiClass::scanNetworks(bool, bool, bool, uint32_t, uint8_t,
                                    const char*, const uint8_t*) {
  ++FakeWiFiState::scanCalls;
  return static_cast<int16_t>(configuredScanNetworks.size());
}

bool FakeWiFiClass::getNetworkInfo(uint8_t networkItem, String& ssid,
                                   uint8_t& encryptionType, int32_t& rssi,
                                   uint8_t*& bssid, int32_t& channel) {
  if (networkItem >= configuredScanNetworks.size()) return false;
  FakeWiFiState::ScanNetwork& network = configuredScanNetworks[networkItem];
  ssid = String(network.ssid);
  encryptionType = 0;
  rssi = network.rssi;
  bssid = network.bssid.data();
  channel = network.channel;
  return true;
}

void FakeWiFiClass::scanDelete() { ++FakeWiFiState::scanDeleteCalls; }

wl_status_t FakeWiFiClass::status() const { return _status; }

String FakeWiFiClass::SSID() const { return _ssid; }

IPAddress FakeWiFiClass::localIP() const { return _ip; }

int32_t FakeWiFiClass::RSSI() const { return _rssi; }

uint8_t* FakeWiFiClass::BSSID() { return _bssid.data(); }

int32_t FakeWiFiClass::channel() const { return _channel; }

void FakeWiFiClass::applyConnectionResult(
    const FakeWiFiState::ConnectionResult& result, uint32_t timeoutMs) {
  fakeMillis += std::min(result.durationMs, timeoutMs);
  if (result.durationMs > timeoutMs) {
    _status = WL_DISCONNECTED;
    return;
  }
  _status = result.status;
  if (_status == WL_CONNECTED) {
    _ssid = String(result.ssid);
    _ip = result.ip;
    _rssi = result.rssi;
    _bssid = result.bssid;
    _channel = result.channel;
  }
}

bool WiFiMulti::addAP(const char* ssid, const char* password) {
  FakeWiFiState::addedAps.push_back(
      {ssid == nullptr ? "" : ssid, password == nullptr ? "" : password});
  return true;
}

uint8_t WiFiMulti::run(uint32_t connectTimeout) {
  ++FakeWiFiState::multiRunCalls;
  FakeWiFiState::applyMultiResult(connectTimeout);
  return static_cast<uint8_t>(WiFi.status());
}

esp_reset_reason_t esp_reset_reason() {
  return FakeWiFiState::resetReason;
}
