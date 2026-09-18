/**
 * @file test_settings_atomic.cpp
 * @brief 原子的な設定保存、世代管理、障害復旧を検証するテスト。
 */

#include <unity.h>
#include "MemorySettingsBackend.h"
#include "settings/AtomicRecordStore.h"

using namespace ArduinoCommon;
#define STATUS(expected, expression) TEST_ASSERT_EQUAL_INT(static_cast<int>(SettingsStatus::expected), static_cast<int>(expression))
const RecordFormat recordFormat{{{'T','E','S','T'}}, 1, 1, 1024};
const RecordSlots first{recordFormat, {{"cfg0", "cfg1"}}};
const RecordSlots second{{{{'T','E','S','T'}}, 1, 2, 1024}, {{"cal0", "cal1"}}};
const RecordSlots roots{{{{'T','E','S','T'}}, 1, 3, 1024}, {{"root0", "root1"}}};
void setUp() {}
void tearDown() {}

/** @brief 初回は全レコードを要求し、metadataを含む組を再起動後に復元する。 */
void test_initial_bundle_and_reload() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first, second}, roots, 1);
  SettingsSnapshot snapshot;
  STATUS(NotFound, store.load(snapshot));
  STATUS(InvalidArgument, store.commit(0, {{0, {1}}}, {9}));
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
  STATUS(Ok, store.commit(0, {{0, {1}}, {1, {2}}}, {9}));
  AtomicRecordStore restarted(backend, {first, second}, roots, 1);
  STATUS(Ok, restarted.load(snapshot));
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.generation);
  TEST_ASSERT_EQUAL_UINT8(1, snapshot.records[0].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(2, snapshot.records[1].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(9, snapshot.metadata[0]);
}

/** @brief 片側変更ではもう片側を書かず、古いrevisionで上書きしない。 */
void test_partial_update_and_conflict() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first, second}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}, {1,{2}}}, {9}));
  const auto calibration = backend.values["cal0"];
  backend.writes = 0;
  STATUS(Ok, store.commit(1, {{0,{3}}}, {8}));
  TEST_ASSERT_EQUAL_INT(2, backend.writes);
  TEST_ASSERT_TRUE(calibration == backend.values["cal0"]);
  STATUS(Conflict, store.commit(1, {{1,{7}}}, {6}));
  TEST_ASSERT_EQUAL_INT(2, backend.writes);
  SettingsSnapshot snapshot;
  STATUS(Ok, store.load(snapshot));
  TEST_ASSERT_EQUAL_UINT8(3, snapshot.records[0].payload[0]);
  TEST_ASSERT_EQUAL_UINT8(2, snapshot.records[1].payload[0]);
}

/** @brief 全書込境界の失敗でも再起動後は旧組または新組で、新旧を混在させない。 */
void test_every_write_boundary_keeps_a_complete_bundle() {
  for (int position = 1; position <= 3; ++position) {
    for (int failureMode = 0; failureMode < 3; ++failureMode) {
      MemorySettingsBackend backend;
      AtomicRecordStore store(backend, {first, second}, roots, 1);
      STATUS(Ok, store.commit(0, {{0,{1}}, {1,{2}}}, {9}));
      backend.writes = 0;
      backend.failWrite = position;
      backend.durableFailure = failureMode == 1;
      backend.failReadback = failureMode == 2;
      const auto result = store.commit(1, {{0,{3}}, {1,{4}}}, {8});
      TEST_ASSERT_EQUAL_INT(static_cast<int>(position == 3 ? SettingsStatus::Indeterminate : SettingsStatus::IoError), static_cast<int>(result));
      backend.readbackPending = false;
      AtomicRecordStore restarted(backend, {first, second}, roots, 1);
      SettingsSnapshot snapshot;
      STATUS(Ok, restarted.load(snapshot));
      const bool adopted = position == 3 && failureMode != 0;
      TEST_ASSERT_EQUAL_UINT8(adopted ? 3 : 1, snapshot.records[0].payload[0]);
      TEST_ASSERT_EQUAL_UINT8(adopted ? 4 : 2, snapshot.records[1].payload[0]);
      TEST_ASSERT_EQUAL_UINT8(adopted ? 8 : 9, snapshot.metadata[0]);
    }
  }
}

/** @brief rootの結果不明後は再照合するまで後続の書込を拒否する。 */
void test_indeterminate_result_requires_reconcile() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}}, {1}));
  backend.writes = 0; backend.failWrite = 2; backend.durableFailure = true;
  STATUS(Indeterminate, store.commit(1, {{0,{2}}}, {2}));
  STATUS(Indeterminate, store.commit(1, {{0,{3}}}, {3}));
  SettingsSnapshot snapshot;
  STATUS(Ok, store.reconcile(snapshot));
  TEST_ASSERT_EQUAL_UINT32(2, snapshot.generation);
  backend.failWrite = 0;
  STATUS(Ok, store.commit(2, {{0,{3}}}, {3}));
}

/** @brief metadataだけの確定でも世代が進み、レコードを再書込しない。 */
void test_metadata_only_commit() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}}, {1}));
  backend.writes = 0;
  STATUS(Ok, store.commit(1, {}, {0}));
  TEST_ASSERT_EQUAL_INT(1, backend.writes);
  SettingsSnapshot snapshot;
  STATUS(Ok, store.load(snapshot));
  TEST_ASSERT_EQUAL_UINT8(0, snapshot.metadata[0]);
}

/** @brief 将来schemaのrootがあれば古いrootへ戻って書込しない。 */
void test_future_root_prevents_fallback_writes() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}}, {1}));
  DecodedRecord record;
  STATUS(Ok, RecordEnvelopeCodec::decode(roots.format, backend.values["root0"], record));
  auto future = roots.format; future.schema = 2;
  STATUS(Ok, RecordEnvelopeCodec::encode(future, 2, record.payload, backend.values["root1"]));
  SettingsSnapshot snapshot;
  STATUS(UnsupportedSchema, store.load(snapshot));
  const int writes = backend.writes;
  STATUS(UnsupportedSchema, store.commit(1, {{0,{3}}}, {3}));
  TEST_ASSERT_EQUAL_INT(writes, backend.writes);
}

/** @brief 最新rootが壊れた場合だけ、参照が揃った旧rootへ復帰する。 */
void test_corrupt_root_falls_back_to_complete_previous() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first,second}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}, {1,{2}}}, {1}));
  STATUS(Ok, store.commit(1, {{0,{3}}, {1,{4}}}, {2}));
  backend.values["root1"].back() ^= 1;
  SettingsSnapshot snapshot;
  STATUS(Ok, store.load(snapshot));
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.generation);
  TEST_ASSERT_EQUAL_UINT8(1, snapshot.records[0].payload[0]);
}

/** @brief 重複キー・範囲外の更新・metadata長超過を保存前に拒否する。 */
void test_invalid_requests_never_write() {
  MemorySettingsBackend backend;
  AtomicRecordStore collision(backend, {first,first}, roots, 1);
  SettingsSnapshot snapshot;
  STATUS(InvalidArgument, collision.load(snapshot));
  AtomicRecordStore store(backend, {first}, roots, 1);
  STATUS(InvalidArgument, store.commit(0, {{0,{1}}, {0,{2}}}, {1}));
  STATUS(InvalidArgument, store.commit(0, {{1,{1}}}, {1}));
  STATUS(InvalidArgument, store.commit(0, {{0,{1}}}, {1,2}));
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

/** @brief 世代の巻き戻りと巨大payloadを、既存データを書き換える前に拒否する。 */
void test_generation_overflow_and_capacity_are_preflight_errors() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first}, roots, 1);
  STATUS(TooLarge, store.commit(0, {{0,SettingsBytes(1001)}}, {1}));
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
  STATUS(Ok, store.commit(0, {{0,{1}}}, {1}));
  DecodedRecord root;
  STATUS(Ok, RecordEnvelopeCodec::decode(roots.format, backend.values["root0"], root));
  STATUS(Ok, RecordEnvelopeCodec::encode(roots.format, 0xffffffffu, root.payload, backend.values["root0"]));
  backend.writes = 0;
  STATUS(GenerationOverflow, store.commit(0xffffffffu, {{0,{2}}}, {2}));
  TEST_ASSERT_EQUAL_INT(0, backend.writes);
}

/** @brief rootだけが正常でも参照先が揃わない世代を採用しない。 */
void test_missing_reference_rejects_new_root() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first,second}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}, {1,{2}}}, {1}));
  STATUS(Ok, store.commit(1, {{0,{3}}, {1,{4}}}, {2}));
  backend.values.erase("cal1");
  SettingsSnapshot snapshot;
  STATUS(Ok, store.load(snapshot));
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.generation);
  backend.values.erase("cal0");
  STATUS(Corrupt, store.load(snapshot));
}

/** @brief サイズ上限でschemaを読めないrootを破損扱いにして上書きしない。 */
void test_oversized_root_blocks_writes() {
  MemorySettingsBackend backend;
  AtomicRecordStore store(backend, {first}, roots, 1);
  STATUS(Ok, store.commit(0, {{0,{1}}}, {1}));
  backend.values["root1"]=SettingsBytes(roots.format.maximumSize+1,0);
  SettingsSnapshot snapshot;
  STATUS(TooLarge, store.load(snapshot));
  const int writes=backend.writes;
  STATUS(TooLarge, store.commit(1,{{0,{2}}},{1}));
  TEST_ASSERT_EQUAL(writes,backend.writes);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_initial_bundle_and_reload);
  RUN_TEST(test_partial_update_and_conflict);
  RUN_TEST(test_every_write_boundary_keeps_a_complete_bundle);
  RUN_TEST(test_indeterminate_result_requires_reconcile);
  RUN_TEST(test_metadata_only_commit);
  RUN_TEST(test_future_root_prevents_fallback_writes);
  RUN_TEST(test_corrupt_root_falls_back_to_complete_previous);
  RUN_TEST(test_invalid_requests_never_write);
  RUN_TEST(test_generation_overflow_and_capacity_are_preflight_errors);
  RUN_TEST(test_missing_reference_rejects_new_root);
  RUN_TEST(test_oversized_root_blocks_writes);
  return UNITY_END();
}
