#pragma once

#include <string>
#include "RecordEnvelopeCodec.h"

namespace ArduinoCommon {

struct RecordSlots {
  RecordFormat format;
  std::array<std::string, 2> keys;
};

struct RecordUpdate {
  size_t index;
  SettingsBytes payload;
};

struct SettingsSnapshot {
  uint32_t generation = 0;
  uint8_t rootSlot = 0;
  std::vector<uint8_t> slots;
  std::vector<DecodedRecord> records;
  SettingsBytes metadata;
};

class AtomicRecordStore {
 public:
  AtomicRecordStore(ISettingsBackend& backend, std::vector<RecordSlots> records,
                   RecordSlots roots, size_t metadataSize);
  SettingsStatus load(SettingsSnapshot& output);
  SettingsStatus reconcile(SettingsSnapshot& output);
  SettingsStatus commit(uint32_t expectedGeneration,
                         const std::vector<RecordUpdate>& updates,
                         const SettingsBytes& metadata);

 private:
  bool validOptions() const;
  ISettingsBackend& _backend;
  std::vector<RecordSlots> _records;
  RecordSlots _roots;
  size_t _metadataSize;
  bool _uncertain = false;
};

}  // namespace ArduinoCommon
