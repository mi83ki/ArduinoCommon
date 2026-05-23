/**
 * BasicUsage - EEPROMStore<T> の最小使用例
 *
 * 1つの構造体を EEPROM に保存し、CRC が一致する場合は前回値を復元します。
 * 初回起動時や破損時はデフォルト値で初期化されます。
 */

#include <EEPROMStore.h>

struct Config {
  uint16_t intervalMs;
  float threshold;
  char deviceName[16];
  bool ledEnabled;
};

Config defaults = {
    1000,
    25.5f,
    "sensor-01",
    true,
};

EEPROMStore<Config> store(0, defaults);

void printConfig() {
  Serial.println(F("--- Current Config ---"));
  Serial.print(F("  Device    : "));
  Serial.println(store.data.deviceName);
  Serial.print(F("  Interval  : "));
  Serial.print(store.data.intervalMs);
  Serial.println(F(" ms"));
  Serial.print(F("  Threshold : "));
  Serial.println(store.data.threshold);
  Serial.print(F("  LED       : "));
  Serial.println(store.data.ledEnabled ? F("ON") : F("OFF"));
  Serial.print(F("  EEPROM    : "));
  Serial.print(store.requiredSize());
  Serial.println(F(" bytes"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  const bool loaded = store.begin();
  Serial.println(
      loaded ? F("Config loaded from EEPROM")
             : F("First boot - initialized with defaults"));
  printConfig();

  store.data.intervalMs = 2000;
  store.data.threshold = 30.0f;

  if (store.save()) {
    Serial.println(F("Config saved (changed)"));
  } else {
    Serial.println(F("Config unchanged, write skipped"));
  }
  printConfig();
}

void loop() {
  if (Serial.available() && Serial.read() == 'r') {
    store.reset();
    Serial.println(F("Reset to defaults"));
    printConfig();
  }
}
