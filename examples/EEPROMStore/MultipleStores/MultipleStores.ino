/**
 * MultipleStores - 複数の EEPROMStore<T> を並べて使う例
 *
 * nextAddress() を使い、構造体ごとに保存領域を重ならないように配置します。
 */

#include <EEPROMStore.h>

struct SensorConfig {
  uint16_t intervalMs;
  float tempThreshold;
  float humidThreshold;
  char name[16];
};

struct NetworkConfig {
  uint8_t ip[4];
  uint16_t port;
  char ssid[32];
  char password[32];
};

SensorConfig sensorDefaults = {
    1000,
    25.5f,
    60.0f,
    "sensor-01",
};

NetworkConfig networkDefaults = {
    {192, 168, 1, 100},
    8080,
    "MyNetwork",
    "password123",
};

EEPROMStore<SensorConfig> sensorStore(0, sensorDefaults, 0xAA01);
EEPROMStore<NetworkConfig> networkStore(sensorStore.nextAddress(),
                                        networkDefaults, 0xAA02);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  const bool sensorLoaded = sensorStore.begin();
  const bool networkLoaded = networkStore.begin();

  Serial.println(sensorLoaded ? F("Sensor  : loaded") : F("Sensor  : defaults"));
  Serial.println(networkLoaded ? F("Network : loaded")
                               : F("Network : defaults"));

  Serial.println(F("\n--- EEPROM Memory Map ---"));
  Serial.print(F("  Sensor  : addr=0, size="));
  Serial.println(sensorStore.requiredSize());
  Serial.print(F("  Network : addr="));
  Serial.print(sensorStore.nextAddress());
  Serial.print(F(", size="));
  Serial.println(networkStore.requiredSize());
  Serial.print(F("  Total   : "));
  Serial.print(networkStore.nextAddress());
  Serial.println(F(" bytes"));

  Serial.println(F("\n--- Sensor Config ---"));
  Serial.print(F("  Name      : "));
  Serial.println(sensorStore.data.name);
  Serial.print(F("  Interval  : "));
  Serial.println(sensorStore.data.intervalMs);
  Serial.print(F("  Temp Th.  : "));
  Serial.println(sensorStore.data.tempThreshold);
  Serial.print(F("  Humid Th. : "));
  Serial.println(sensorStore.data.humidThreshold);

  Serial.println(F("\n--- Network Config ---"));
  Serial.print(F("  IP   : "));
  for (int i = 0; i < 4; i++) {
    if (i > 0) {
      Serial.print('.');
    }
    Serial.print(networkStore.data.ip[i]);
  }
  Serial.println();
  Serial.print(F("  Port : "));
  Serial.println(networkStore.data.port);
  Serial.print(F("  SSID : "));
  Serial.println(networkStore.data.ssid);
}

void loop() {}
