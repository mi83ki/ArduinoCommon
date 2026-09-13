#pragma once
#include "WiFiProfileValidator.h"
#include "ApCredentialStore.h"
#include <atomic>

namespace ArduinoCommon {
enum class WiFiProbeState {Idle,Connecting,Succeeded,Failed,Cancelled};
struct WiFiProbeResult {
  uint64_t jobId=0;
  uint8_t profileId=0;
  WiFiProbeState state=WiFiProbeState::Idle;
  IPv4 address{};
  std::string error;
  bool apSuspended=false;
};
struct WiFiScanEntry {std::string ssid;int32_t rssi=0;bool open=false;};
enum class WiFiScanState {Idle,Scanning,Ready,Failed};
class WiFiProvisioningProbe {
 public:
  bool begin(const ApCredentials&,IPv4 apAddress={{192,168,4,1}});
  void stop();
  bool start(const WiFiProfile&,uint64_t jobId,uint32_t deadlineMillis);
  void poll();
  bool cancel(uint64_t jobId);
  bool finish(uint64_t jobId);
  WiFiProbeResult result() const;
  bool apAvailable() const;
  bool startScan();
  WiFiScanState scanState() const;
  std::vector<WiFiScanEntry> scanResults() const;
 private:
  bool restoreAp();
  void fail(const char* error);
  ApCredentials _credentials;
  IPv4 _apAddress{};
  WiFiProfile _profile;
  WiFiProbeResult _result;
  bool _started=false,_stationOwned=false;
  std::atomic<bool> _apAvailable{false};
  uint32_t _startedAt=0,_timeout=0,_scanAt=0,_scanCompletedAt=0;
  WiFiScanState _scanState=WiFiScanState::Idle;
  std::vector<WiFiScanEntry> _scanResults;
};
}
