#include "AtomicRecordStore.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace ArduinoCommon {
namespace {

/** @brief 採用情報の固定幅整数を読み出す。 */
uint32_t get32(const SettingsBytes& bytes, size_t offset) {
  return uint32_t(bytes[offset]) | (uint32_t(bytes[offset+1]) << 8) |
         (uint32_t(bytes[offset+2]) << 16) | (uint32_t(bytes[offset+3]) << 24);
}
/** @brief 採用情報へ固定幅整数を書き込む。 */
void set32(SettingsBytes& bytes, size_t offset, uint32_t value) {
  for (unsigned i=0; i<4; ++i) bytes[offset+i] = uint8_t(value >> (i*8));
}

/** @brief 読込失敗のうち、別rootへフォールバックしてよいものを判定する。 */
bool invalidRecord(SettingsStatus status) {
  return status == SettingsStatus::NotFound || status == SettingsStatus::Corrupt;
}
}  // namespace

/** @brief 外部backendと1～2個の論理レコードを保持し、生成時には保存しない。 */
AtomicRecordStore::AtomicRecordStore(ISettingsBackend& backend,
    std::vector<RecordSlots> records, RecordSlots roots, size_t metadataSize)
    : _backend(backend), _records(std::move(records)), _roots(std::move(roots)),
      _metadataSize(metadataSize) {}

/** @brief キーの衝突・形式・root容量をアクセス前に検証する。 */
bool AtomicRecordStore::validOptions() const {
  if (_records.empty() || _records.size() > 2 || _roots.format.maximumSize < 24)
    return false;
  const size_t referenceSize = 9 * _records.size();
  if (_roots.format.maximumSize - 24 < referenceSize ||
      _metadataSize > _roots.format.maximumSize - 24 - referenceSize) return false;
  std::set<std::string> names;
  auto valid = [&names](const RecordSlots& slots) {
    if (!slots.format.schema || slots.format.maximumSize < 24) return false;
    for (const auto& key : slots.keys) {
      if (key.empty() || key.size() > 15 || !names.insert(key).second) return false;
      for (unsigned char ch : key) if (ch < 32 || ch > 126) return false;
    }
    return true;
  };
  if (!valid(_roots)) return false;
  for (const auto& record : _records) if (!valid(record)) return false;
  return true;
}

/**
 * @brief 参照が完全なrootのうち最新のものを採用する。
 * @param output 成功時にだけ置き換えるスナップショット
 * @return SettingsStatus 未対応schemaやIO障害は旧rootへ戻して隠さない
 */
SettingsStatus AtomicRecordStore::load(SettingsSnapshot& output) {
  if (!validOptions()) return SettingsStatus::InvalidArgument;
  SettingsSnapshot best;
  bool anyRoot = false;
  for (uint8_t rootSlot = 0; rootSlot < 2; ++rootSlot) {
    SettingsBytes bytes;
    auto status = _backend.read(_roots.keys[rootSlot].c_str(), bytes, _roots.format.maximumSize);
    if (status == SettingsStatus::NotFound) continue;
    anyRoot = true;
    if (invalidRecord(status)) continue;
    if (status != SettingsStatus::Ok) return status;
    DecodedRecord root;
    status = RecordEnvelopeCodec::decode(_roots.format, bytes, root);
    if (invalidRecord(status)) continue;
    if (status != SettingsStatus::Ok) return status;
    const size_t referenceSize = 9 * _records.size();
    if (root.payload.size() != referenceSize + _metadataSize) continue;
    SettingsSnapshot candidate;
    candidate.generation = root.generation;
    candidate.rootSlot = rootSlot;
    candidate.metadata.assign(root.payload.begin() + referenceSize, root.payload.end());
    bool complete = true;
    for (size_t i=0; i<_records.size(); ++i) {
      const size_t offset = i * 9;
      const uint8_t slot = root.payload[offset];
      const uint32_t generation = get32(root.payload, offset + 1);
      const uint32_t crc = get32(root.payload, offset + 5);
      if (slot > 1 || !generation || generation > root.generation) { complete = false; break; }
      status = _backend.read(_records[i].keys[slot].c_str(), bytes, _records[i].format.maximumSize);
      if (invalidRecord(status)) { complete = false; break; }
      if (status != SettingsStatus::Ok) return status;
      DecodedRecord record;
      status = RecordEnvelopeCodec::decode(_records[i].format, bytes, record);
      if (invalidRecord(status)) { complete = false; break; }
      if (status != SettingsStatus::Ok) return status;
      if (record.generation != generation || record.crc != crc) { complete = false; break; }
      candidate.slots.push_back(slot);
      candidate.records.push_back(std::move(record));
    }
    if (!complete) continue;
    if (candidate.generation == best.generation) return SettingsStatus::Corrupt;
    if (candidate.generation > best.generation) best = std::move(candidate);
  }
  if (!best.generation) return anyRoot ? SettingsStatus::Corrupt : SettingsStatus::NotFound;
  output = std::move(best);
  return SettingsStatus::Ok;
}

/** @brief 永続データを再照合し、結果が確定した時だけ後続保存を許可する。 */
SettingsStatus AtomicRecordStore::reconcile(SettingsSnapshot& output) {
  const auto status = load(output);
  if (status == SettingsStatus::Ok || status == SettingsStatus::NotFound) _uncertain = false;
  return status;
}

/**
 * @brief 変更レコードを非採用スロットへ先に保存し、rootを最後に確定する。
 * @param expectedGeneration 呼出元が編集したroot世代。初回のみ0
 * @param updates 変更する論理レコードとpayload
 * @param metadata 製品が意味を解釈する固定長の付帯情報
 * @return SettingsStatus rootの保存成否が不明ならIndeterminateを返す
 */
SettingsStatus AtomicRecordStore::commit(uint32_t expectedGeneration,
    const std::vector<RecordUpdate>& updates, const SettingsBytes& metadata) {
  if (_uncertain) return SettingsStatus::Indeterminate;
  if (!validOptions() || metadata.size() != _metadataSize) return SettingsStatus::InvalidArgument;
  SettingsSnapshot current;
  auto status = load(current);
  if (status != SettingsStatus::Ok && status != SettingsStatus::NotFound) return status;
  const bool initial = status == SettingsStatus::NotFound;
  if (current.generation != expectedGeneration) return SettingsStatus::Conflict;
  if (current.generation == std::numeric_limits<uint32_t>::max()) return SettingsStatus::GenerationOverflow;
  if (initial) {
    current.slots.resize(_records.size(), 1);
    current.records.resize(_records.size());
  }
  const uint32_t nextGeneration = current.generation + 1;
  std::vector<bool> changed(_records.size(), false);
  std::vector<SettingsBytes> encoded(_records.size());
  for (const auto& update : updates) {
    if (update.index >= _records.size() || changed[update.index]) return SettingsStatus::InvalidArgument;
    changed[update.index] = true;
    status = RecordEnvelopeCodec::encode(_records[update.index].format, nextGeneration,
                                         update.payload, encoded[update.index]);
    if (status != SettingsStatus::Ok) return status;
  }
  if (initial && updates.size() != _records.size()) return SettingsStatus::InvalidArgument;
  SettingsBytes rootPayload(9 * _records.size() + metadata.size());
  for (size_t i=0; i<_records.size(); ++i) {
    if (changed[i]) {
      current.slots[i] ^= 1;
      status = RecordEnvelopeCodec::decode(_records[i].format, encoded[i], current.records[i]);
      if (status != SettingsStatus::Ok) return status;
    }
    rootPayload[i*9] = current.slots[i];
    set32(rootPayload, i*9+1, current.records[i].generation);
    set32(rootPayload, i*9+5, current.records[i].crc);
  }
  std::copy(metadata.begin(), metadata.end(), rootPayload.begin() + 9 * _records.size());
  SettingsBytes rootBytes;
  status = RecordEnvelopeCodec::encode(_roots.format, nextGeneration, rootPayload, rootBytes);
  if (status != SettingsStatus::Ok) return status;
  for (size_t i=0; i<_records.size(); ++i) {
    if (!changed[i]) continue;
    const char* key = _records[i].keys[current.slots[i]].c_str();
    status = _backend.write(key, encoded[i]);
    if (status != SettingsStatus::Ok) return status;
    SettingsBytes readback;
    if (_backend.read(key, readback, _records[i].format.maximumSize) != SettingsStatus::Ok ||
        readback != encoded[i]) return SettingsStatus::IoError;
  }
  const uint8_t rootSlot = initial ? 0 : uint8_t(current.rootSlot ^ 1);
  const char* rootKey = _roots.keys[rootSlot].c_str();
  _uncertain = true;
  if (_backend.write(rootKey, rootBytes) != SettingsStatus::Ok) return SettingsStatus::Indeterminate;
  SettingsBytes readback;
  if (_backend.read(rootKey, readback, _roots.format.maximumSize) != SettingsStatus::Ok ||
      readback != rootBytes) return SettingsStatus::Indeterminate;
  _uncertain = false;
  return SettingsStatus::Ok;
}

}  // namespace ArduinoCommon
