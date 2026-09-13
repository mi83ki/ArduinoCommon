#include "RecordEnvelopeCodec.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ArduinoCommon {
namespace {

/** @brief 固定幅整数をホストのアラインメントに依存せず読み出す。 */
uint32_t read32(const SettingsBytes& bytes, size_t offset) {
  return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
         (uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
}

/** @brief 固定幅整数をlittle-endianで格納する。 */
void write32(SettingsBytes& bytes, size_t offset, uint32_t value) {
  for (size_t i = 0; i < 4; ++i) bytes[offset + i] = uint8_t(value >> (8 * i));
}

/** @brief CRC欄をゼロと見なしてレコード全体のCRC32/ISO-HDLCを計算する。 */
uint32_t recordCrc(const SettingsBytes& bytes) {
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < bytes.size(); ++i) {
    crc ^= (i >= 16 && i < 20) ? 0 : bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
  }
  return crc ^ 0xffffffffu;
}

}  // namespace

/**
 * @brief payloadを固定ヘッダーとCRC付きで符号化する。
 * @param format 製品が指定するmagic・schema・種別・上限
 * @param generation 1以上の保存世代
 * @param payload 製品側で符号化した内容
 * @param output 成功した場合だけ置き換える出力
 * @return SettingsStatus 成功または引数・容量のエラー
 */
SettingsStatus RecordEnvelopeCodec::encode(const RecordFormat& format,
    uint32_t generation, const SettingsBytes& payload, SettingsBytes& output) {
  if (!generation || !format.schema || format.maximumSize < kHeaderSize)
    return SettingsStatus::InvalidArgument;
  if (payload.size() > format.maximumSize - kHeaderSize ||
      payload.size() > std::numeric_limits<uint32_t>::max())
    return SettingsStatus::TooLarge;
  SettingsBytes bytes(kHeaderSize + payload.size(), 0);
  std::copy(format.magic.begin(), format.magic.end(), bytes.begin());
  bytes[4] = uint8_t(format.schema);
  bytes[5] = uint8_t(format.schema >> 8);
  bytes[6] = format.kind;
  write32(bytes, 8, generation);
  write32(bytes, 12, static_cast<uint32_t>(payload.size()));
  std::copy(payload.begin(), payload.end(), bytes.begin() + kHeaderSize);
  write32(bytes, 16, recordCrc(bytes));
  output = std::move(bytes);
  return SettingsStatus::Ok;
}

/**
 * @brief 長さ・同一性・CRCを検証してからレコードを復元する。
 * @param format 採用できる形式と最大サイズ
 * @param bytes 保存された完全なバイト列
 * @param output 検証に成功した場合だけ置き換える復元結果
 * @return SettingsStatus 未対応schemaは破損と区別して通知する
 */
SettingsStatus RecordEnvelopeCodec::decode(const RecordFormat& format,
    const SettingsBytes& bytes, DecodedRecord& output) {
  if (!format.schema || format.maximumSize < kHeaderSize)
    return SettingsStatus::InvalidArgument;
  if (bytes.size() > format.maximumSize) return SettingsStatus::TooLarge;
  if (bytes.size() < kHeaderSize ||
      !std::equal(format.magic.begin(), format.magic.end(), bytes.begin()) ||
      bytes[6] != format.kind || read32(bytes, 12) != bytes.size() - kHeaderSize ||
      read32(bytes, 16) != recordCrc(bytes)) return SettingsStatus::Corrupt;
  const uint16_t schema = uint16_t(bytes[4]) | (uint16_t(bytes[5]) << 8);
  if (schema != format.schema) return SettingsStatus::UnsupportedSchema;
  if (bytes[7] || read32(bytes, 20) || !read32(bytes, 8)) return SettingsStatus::Corrupt;
  DecodedRecord decoded;
  decoded.generation = read32(bytes, 8);
  decoded.crc = read32(bytes, 16);
  decoded.payload.assign(bytes.begin() + kHeaderSize, bytes.end());
  output = std::move(decoded);
  return SettingsStatus::Ok;
}

}  // namespace ArduinoCommon
