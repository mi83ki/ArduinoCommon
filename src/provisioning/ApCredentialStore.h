#pragma once
#include <array>
#include <functional>
#include <string>
#include "../settings/RecordEnvelopeCodec.h"

namespace ArduinoCommon {
struct ApCredentials {std::string ssid,password;};
class ApCredentialStore {
 public:
  using RandomFill=std::function<bool(uint8_t*,size_t)>;
  ApCredentialStore(ISettingsBackend&,std::string prefix,std::array<uint8_t,6> mac,RandomFill);
  SettingsStatus load(ApCredentials& output);
  SettingsStatus loadOrCreate(ApCredentials& output);
  SettingsStatus regenerate(ApCredentials& output);
  static std::string wifiQr(const ApCredentials& credentials);
 private:
  ISettingsBackend& _backend;
  std::string _prefix;
  std::array<uint8_t,6> _mac;
  RandomFill _random;
};
}
