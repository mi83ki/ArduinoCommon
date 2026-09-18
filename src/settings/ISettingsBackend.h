#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ArduinoCommon {

using SettingsBytes = std::vector<uint8_t>;

enum class SettingsStatus {
  Ok, NotFound, InvalidArgument, TooLarge, ReadOnly, IoError,
  Corrupt, UnsupportedSchema, Conflict, Indeterminate, GenerationOverflow
};

class ISettingsBackend {
 public:
  virtual ~ISettingsBackend() = default;
  virtual SettingsStatus read(const char* key, SettingsBytes& output,
                              size_t limit) = 0;
  virtual SettingsStatus write(const char* key, const SettingsBytes& bytes) = 0;
  virtual SettingsStatus remove(const char* key) = 0;
};

}  // namespace ArduinoCommon
