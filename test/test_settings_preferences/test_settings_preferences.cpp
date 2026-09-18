/**
 * @file test_settings_preferences.cpp
 * @brief Preferences/NVSバックエンドの分離・読取専用・障害処理を検証するテスト。
 */

#include <unity.h>
#include "nvs.h"
#include "settings/PreferencesBackend.h"

using namespace ArduinoCommon;
void setUp() { FakeNvs::reset(); }
void tearDown() { TEST_ASSERT_TRUE(FakeNvs::state().handles.empty()); }

#define STATUS(expected, expression) TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::expected), static_cast<int>(expression))

/** @brief 不存在の読み出しでnamespaceや出力を変更しない。 */
void test_missing_read_never_initializes_storage() {
  PreferencesBackend backend("settings", true);
  SettingsBytes output{9};
  STATUS(NotFound, backend.read("cfg", output, 1024));
  TEST_ASSERT_TRUE(FakeNvs::state().spaces.empty());
  TEST_ASSERT_EQUAL_UINT8(9, output[0]);
}

/** @brief 指定namespaceとキーだけに保存して再読込できる。 */
void test_roundtrip_keeps_namespaces_independent() {
  PreferencesBackend a("a"), b("b");
  STATUS(Ok, a.write("cfg", {1,2,3}));
  STATUS(Ok, b.write("cfg", {4}));
  SettingsBytes bytes;
  STATUS(Ok, a.read("cfg", bytes, 3));
  const uint8_t expected[]{1,2,3};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, bytes.data(), 3);
  STATUS(Ok, a.remove("cfg"));
  STATUS(NotFound, a.read("cfg", bytes, 3));
  STATUS(Ok, b.read("cfg", bytes, 3));
  TEST_ASSERT_EQUAL_UINT8(4, bytes[0]);
}

/** @brief 読取専用では保存も削除もせず旧blobを保持する。 */
void test_readonly_rejects_all_mutations() {
  FakeNvs::state().spaces["eeprom"]["eeprom"] = {true, {1,2}};
  PreferencesBackend backend("eeprom", true);
  STATUS(ReadOnly, backend.write("eeprom", {3}));
  STATUS(ReadOnly, backend.remove("eeprom"));
  TEST_ASSERT_EQUAL_UINT32(0, FakeNvs::state().writeCalls);
  TEST_ASSERT_EQUAL_UINT8(1, FakeNvs::state().spaces["eeprom"]["eeprom"].bytes[0]);
}

/** @brief namespace障害・型相違を不存在と混同しない。 */
void test_distinguishes_io_failure_and_wrong_type() {
  PreferencesBackend backend("settings");
  SettingsBytes bytes{7};
  FakeNvs::state().openError = true;
  STATUS(IoError, backend.read("cfg", bytes, 1024));
  FakeNvs::state().openError = false;
  FakeNvs::state().spaces["settings"]["cfg"] = {false, {}};
  STATUS(Corrupt, backend.read("cfg", bytes, 1024));
  FakeNvs::state().readError = true;
  STATUS(IoError, backend.read("cfg", bytes, 1024));
  TEST_ASSERT_EQUAL_UINT8(7, bytes[0]);
}

/** @brief 上限超過をデータ確保・取得より前に検出する。 */
void test_read_limit_prevents_payload_read() {
  FakeNvs::state().spaces["settings"]["cfg"] = {true, {1,2,3}};
  PreferencesBackend backend("settings");
  SettingsBytes bytes{7};
  STATUS(TooLarge, backend.read("cfg", bytes, 2));
  TEST_ASSERT_EQUAL_UINT32(0, FakeNvs::state().readCalls);
  TEST_ASSERT_EQUAL_UINT8(7, bytes[0]);
}

/** @brief 名前の空文字・16文字以上・非ASCIIを保存前に拒否する。 */
void test_validates_names_before_opening() {
  for (const char* invalid : {"", "1234567890123456", "\xc3\xa9"}) {
    PreferencesBackend bad(invalid), good("valid");
    SettingsBytes bytes;
    STATUS(InvalidArgument, bad.read("cfg", bytes, 10));
    STATUS(InvalidArgument, good.write(invalid, {1}));
  }
  PreferencesBackend maximum("123456789012345");
  STATUS(Ok, maximum.write("123456789012345", {1}));
  STATUS(InvalidArgument, maximum.write(nullptr, {1}));
}

/** @brief 保存・削除失敗で成功を返さず、既存値を保持する。 */
void test_reports_write_and_remove_failure() {
  PreferencesBackend backend("settings");
  STATUS(Ok, backend.write("cfg", {1}));
  FakeNvs::state().writeError = true;
  STATUS(IoError, backend.write("cfg", {2}));
  FakeNvs::state().removeError = true;
  STATUS(IoError, backend.remove("cfg"));
  STATUS(NotFound, backend.remove("missing"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_missing_read_never_initializes_storage);
  RUN_TEST(test_roundtrip_keeps_namespaces_independent);
  RUN_TEST(test_readonly_rejects_all_mutations);
  RUN_TEST(test_distinguishes_io_failure_and_wrong_type);
  RUN_TEST(test_read_limit_prevents_payload_read);
  RUN_TEST(test_validates_names_before_opening);
  RUN_TEST(test_reports_write_and_remove_failure);
  return UNITY_END();
}
