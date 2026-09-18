#pragma once
#include <map>
#include <string>
#include "settings/ISettingsBackend.h"

class MemorySettingsBackend : public ArduinoCommon::ISettingsBackend {
 public:
  std::map<std::string, ArduinoCommon::SettingsBytes> values;
  int writes = 0;
  int failWrite = 0;
  bool durableFailure = false;
  bool failReadback = false;
  bool readbackPending = false;

  ArduinoCommon::SettingsStatus read(const char* key, ArduinoCommon::SettingsBytes& out, size_t limit) override {
    using ArduinoCommon::SettingsStatus;
    if (readbackPending) { readbackPending = false; return SettingsStatus::IoError; }
    auto found = values.find(key);
    if (found == values.end()) return SettingsStatus::NotFound;
    if (found->second.size() > limit) return SettingsStatus::TooLarge;
    out = found->second;
    return SettingsStatus::Ok;
  }
  ArduinoCommon::SettingsStatus write(const char* key, const ArduinoCommon::SettingsBytes& bytes) override {
    using ArduinoCommon::SettingsStatus;
    const bool fail = ++writes == failWrite;
    if (!fail || durableFailure || failReadback) values[key] = bytes;
    if (fail && failReadback) { readbackPending = true; return SettingsStatus::Ok; }
    return fail ? SettingsStatus::IoError : SettingsStatus::Ok;
  }
  ArduinoCommon::SettingsStatus remove(const char* key) override {
    return values.erase(key) ? ArduinoCommon::SettingsStatus::Ok : ArduinoCommon::SettingsStatus::NotFound;
  }
};
