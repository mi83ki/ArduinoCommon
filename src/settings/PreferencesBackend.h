#pragma once

#include <string>
#include "ISettingsBackend.h"

namespace ArduinoCommon {

class PreferencesBackend : public ISettingsBackend {
 public:
  explicit PreferencesBackend(const char* nameSpace, bool readOnly = false);
  SettingsStatus read(const char* key, SettingsBytes& output, size_t limit) override;
  SettingsStatus write(const char* key, const SettingsBytes& bytes) override;
  SettingsStatus remove(const char* key) override;

 private:
  std::string _namespace;
  bool _readOnly;
};

}  // namespace ArduinoCommon
