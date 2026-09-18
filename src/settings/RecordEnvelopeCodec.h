#pragma once

#include <array>
#include "ISettingsBackend.h"

namespace ArduinoCommon {

struct RecordFormat {
  std::array<uint8_t, 4> magic;
  uint16_t schema;
  uint8_t kind;
  size_t maximumSize;
};

struct DecodedRecord {
  uint32_t generation = 0;
  uint32_t crc = 0;
  SettingsBytes payload;
};

class RecordEnvelopeCodec {
 public:
  static constexpr size_t kHeaderSize = 24;
  static SettingsStatus encode(const RecordFormat& format, uint32_t generation,
                               const SettingsBytes& payload, SettingsBytes& output);
  static SettingsStatus decode(const RecordFormat& format,
                               const SettingsBytes& bytes, DecodedRecord& output);
};

}  // namespace ArduinoCommon
