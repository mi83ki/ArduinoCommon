/**
 * BasicUsage - EEPROMStore の基本的な使い方
 *
 * 構造体をEEPROMに保存・読み込みする最小限のサンプル。
 * 初回起動時はデフォルト値で初期化され、2回目以降は前回の値が復元される。
 * シリアルモニタで 'r' を送信するとデフォルトにリセット。
 */

#include <EEPROMStore.h>

struct Config {
    uint16_t intervalMs;
    float    threshold;
    char     deviceName[16];
    bool     ledEnabled;
};

Config defaults = {
    1000,
    25.5f,
    "sensor-01",
    true
};

EEPROMStore<Config> store(0, defaults);

void printConfig() {
    Serial.println(F("--- Current Config ---"));
    Serial.print(F("  Device    : ")); Serial.println(store.data.deviceName);
    Serial.print(F("  Interval  : ")); Serial.print(store.data.intervalMs); Serial.println(F(" ms"));
    Serial.print(F("  Threshold : ")); Serial.println(store.data.threshold);
    Serial.print(F("  LED       : ")); Serial.println(store.data.ledEnabled ? F("ON") : F("OFF"));
    Serial.print(F("  EEPROM    : ")); Serial.print(store.requiredSize()); Serial.println(F(" bytes"));
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    bool loaded = store.begin();
    Serial.println(loaded ? F("Config loaded from EEPROM") : F("First boot - initialized with defaults"));
    printConfig();

    // 値を変更して保存
    store.data.intervalMs = 2000;
    store.data.threshold  = 30.0f;

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
