#include <Arduino.h>
#include "settings/AtomicRecordStore.h"
#include "settings/PreferencesBackend.h"

using namespace ArduinoCommon;

PreferencesBackend backend("ac_example");
const RecordSlots config{{{{'A','C','S','T'}}, 1, 1, 1024}, {{"cfg0", "cfg1"}}};
const RecordSlots root{{{{'A','C','S','T'}}, 1, 2, 1024}, {{"root0", "root1"}}};
AtomicRecordStore store(backend, {config}, root, 0);

/** @brief 保存値を表示する。破損・未対応形式の場合は自動初期化しない。 */
void showSettings() {
  SettingsSnapshot snapshot;
  const auto result = store.reconcile(snapshot);
  Serial.printf("load status=%d\n", static_cast<int>(result));
  if (result == SettingsStatus::Ok && !snapshot.records[0].payload.empty())
    Serial.printf("revision=%lu value=%u\n", static_cast<unsigned long>(snapshot.generation),
                  snapshot.records[0].payload[0]);
}

/** @brief 初回だけ既定値を保存し、以後は確定済みの設定を使う。 */
void setup() {
  Serial.begin(115200);
  SettingsSnapshot snapshot;
  if (store.load(snapshot) == SettingsStatus::NotFound)
    Serial.printf("initialize status=%d\n", static_cast<int>(store.commit(0, {{0,{0}}}, {})));
  Serial.println("r: read, s: increment and save. Restart to verify persistence.");
  showSettings();
}

/** @brief 明示操作時だけ保存し、書込失敗を成功と表示しない。 */
void loop() {
  if (Serial.available()) {
    const char command = static_cast<char>(Serial.read());
    if (command == 's') {
      SettingsSnapshot snapshot;
      if (store.reconcile(snapshot) == SettingsStatus::Ok && !snapshot.records[0].payload.empty()) {
        const uint8_t next = uint8_t(snapshot.records[0].payload[0] + 1);
        Serial.printf("save status=%d\n", static_cast<int>(store.commit(snapshot.generation, {{0,{next}}}, {})));
      }
    }
    if (command == 'r' || command == 's') showSettings();
  }
  delay(10);
}
