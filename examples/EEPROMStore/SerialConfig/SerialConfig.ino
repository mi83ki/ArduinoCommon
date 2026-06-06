/**
 * SerialConfig - シリアル入力で設定を変更して EEPROM に保存する例
 *
 * コマンド:
 *   show
 *   name <value>
 *   interval <ms>
 *   offset <value>
 *   loglevel <0-4>
 *   save
 *   reset
 */

#include <EEPROMStore.h>

struct DeviceConfig {
  char name[20];
  uint32_t intervalMs;
  float calibOffset;
  uint8_t logLevel;
};

DeviceConfig defaults = {
    "my-device",
    5000,
    0.0f,
    3,
};

EEPROMStore<DeviceConfig> config(0, defaults);

static char lineBuf[64];
static uint8_t linePos = 0;

void printConfig() {
  Serial.println(F("\n=== Device Config ==="));
  Serial.print(F("  name     : "));
  Serial.println(config.data.name);
  Serial.print(F("  interval : "));
  Serial.print(config.data.intervalMs);
  Serial.println(F(" ms"));
  Serial.print(F("  offset   : "));
  Serial.println(config.data.calibOffset, 3);
  Serial.print(F("  logLevel : "));
  Serial.println(config.data.logLevel);
  Serial.println();
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  show              Show current config"));
  Serial.println(F("  name <value>      Set device name (max 19 chars)"));
  Serial.println(F("  interval <ms>     Set interval in ms"));
  Serial.println(F("  offset <value>    Set calibration offset"));
  Serial.println(F("  loglevel <0-4>    Set log level"));
  Serial.println(F("  save              Save to EEPROM"));
  Serial.println(F("  reset             Reset to defaults"));
}

void processCommand(const char* line) {
  if (strncmp(line, "show", 4) == 0) {
    printConfig();
  } else if (strncmp(line, "name ", 5) == 0) {
    strncpy(config.data.name, line + 5, sizeof(config.data.name) - 1);
    config.data.name[sizeof(config.data.name) - 1] = '\0';
    Serial.print(F("name -> "));
    Serial.println(config.data.name);
  } else if (strncmp(line, "interval ", 9) == 0) {
    config.data.intervalMs = atol(line + 9);
    Serial.print(F("interval -> "));
    Serial.println(config.data.intervalMs);
  } else if (strncmp(line, "offset ", 7) == 0) {
    config.data.calibOffset = atof(line + 7);
    Serial.print(F("offset -> "));
    Serial.println(config.data.calibOffset, 3);
  } else if (strncmp(line, "loglevel ", 9) == 0) {
    config.data.logLevel = constrain(atoi(line + 9), 0, 4);
    Serial.print(F("logLevel -> "));
    Serial.println(config.data.logLevel);
  } else if (strncmp(line, "save", 4) == 0) {
    if (config.save()) {
      Serial.println(F("Saved to EEPROM"));
    } else {
      Serial.println(F("No changes, write skipped"));
    }
  } else if (strncmp(line, "reset", 5) == 0) {
    config.reset();
    Serial.println(F("Reset to defaults and saved"));
    printConfig();
  } else {
    printHelp();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  const bool loaded = config.begin();
  Serial.println(loaded ? F("Config loaded from EEPROM")
                        : F("Initialized with defaults"));
  printConfig();
  printHelp();
  Serial.print(F("> "));
}

void loop() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (linePos > 0) {
        lineBuf[linePos] = '\0';
        processCommand(lineBuf);
        linePos = 0;
        Serial.print(F("> "));
      }
    } else if (linePos < sizeof(lineBuf) - 1) {
      lineBuf[linePos++] = c;
    }
  }
}
