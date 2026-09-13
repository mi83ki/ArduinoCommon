#include <unity.h>
#include "settings/RecordEnvelopeCodec.h"

using namespace ArduinoCommon;

const RecordFormat format{{{'B', 'E', 'C', 'F'}}, 1, 2, 1024};
const SettingsBytes golden{
    66, 69, 67, 70, 1, 0, 2, 0, 4, 3, 2, 1, 3, 0, 0, 0,
    231, 34, 245, 186, 0, 0, 0, 0, 10, 20, 30};

void setUp() {}
void tearDown() {}

/** @brief 固定バイト列との比較でLE形式と独立計算したCRCを検証する。 */
void test_encodes_fixed_wire_format() {
  SettingsBytes bytes;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Ok),
      static_cast<int>(RecordEnvelopeCodec::encode(format, 0x01020304, {10,20,30}, bytes)));
  TEST_ASSERT_EQUAL_UINT32(golden.size(), bytes.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(golden.data(), bytes.data(), golden.size());
}

/** @brief 既知のレコードから世代・CRC・payloadを復元する。 */
void test_decodes_independent_fixture() {
  DecodedRecord record;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Ok),
      static_cast<int>(RecordEnvelopeCodec::decode(format, golden, record)));
  TEST_ASSERT_EQUAL_HEX32(0x01020304, record.generation);
  TEST_ASSERT_EQUAL_HEX32(0xbaf522e7, record.crc);
  const uint8_t expected[]{10,20,30};
  TEST_ASSERT_EQUAL_UINT32(3, record.payload.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, record.payload.data(), 3);
}

/** @brief 任意位置の破損を検出し、失敗時に出力を変更しない。 */
void test_rejects_corruption_without_changing_output() {
  for (size_t i = 0; i < golden.size(); ++i) {
    SettingsBytes broken = golden;
    broken[i] ^= 0x80;
    DecodedRecord record{99, 123, {7}};
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(SettingsStatus::Ok),
        static_cast<int>(RecordEnvelopeCodec::decode(format, broken, record)));
    TEST_ASSERT_EQUAL_UINT32(99, record.generation);
    TEST_ASSERT_EQUAL_UINT32(1, record.payload.size());
    TEST_ASSERT_EQUAL_UINT8(7, record.payload[0]);
  }
}

/** @brief 部分レコードや余分な末尾を受理しない。 */
void test_rejects_incomplete_and_trailing_bytes() {
  DecodedRecord record;
  for (size_t length = 0; length < golden.size(); ++length) {
    SettingsBytes shortBytes(golden.begin(), golden.begin() + length);
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(SettingsStatus::Ok),
        static_cast<int>(RecordEnvelopeCodec::decode(format, shortBytes, record)));
  }
  SettingsBytes extra = golden;
  extra.push_back(0);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Corrupt),
      static_cast<int>(RecordEnvelopeCodec::decode(format, extra, record)));
}

/** @brief 正常CRCの将来schemaを破損と区別する。 */
void test_detects_newer_schema() {
  RecordFormat future = format;
  future.schema = 2;
  SettingsBytes bytes;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Ok),
      static_cast<int>(RecordEnvelopeCodec::encode(future, 1, {}, bytes)));
  DecodedRecord record;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::UnsupportedSchema),
      static_cast<int>(RecordEnvelopeCodec::decode(format, bytes, record)));
}

/** @brief 上限に収まる最大payloadを扱い、超過・世代0を拒否する。 */
void test_enforces_size_and_generation() {
  SettingsBytes bytes;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Ok),
      static_cast<int>(RecordEnvelopeCodec::encode(format, 1, SettingsBytes(1000, 1), bytes)));
  DecodedRecord record;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Ok),
      static_cast<int>(RecordEnvelopeCodec::decode(format, bytes, record)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::TooLarge),
      static_cast<int>(RecordEnvelopeCodec::encode(format, 1, SettingsBytes(1001), bytes)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::InvalidArgument),
      static_cast<int>(RecordEnvelopeCodec::encode(format, 0, {}, bytes)));
}

/** @brief 異なる種別とmagicをレコードとして採用しない。 */
void test_rejects_other_record_identity() {
  RecordFormat wrong = format;
  wrong.kind = 3;
  DecodedRecord record;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Corrupt),
      static_cast<int>(RecordEnvelopeCodec::decode(wrong, golden, record)));
  wrong = format;
  wrong.magic[0] = 'X';
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::Corrupt),
      static_cast<int>(RecordEnvelopeCodec::decode(wrong, golden, record)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_encodes_fixed_wire_format);
  RUN_TEST(test_decodes_independent_fixture);
  RUN_TEST(test_rejects_corruption_without_changing_output);
  RUN_TEST(test_rejects_incomplete_and_trailing_bytes);
  RUN_TEST(test_detects_newer_schema);
  RUN_TEST(test_enforces_size_and_generation);
  RUN_TEST(test_rejects_other_record_identity);
  return UNITY_END();
}
