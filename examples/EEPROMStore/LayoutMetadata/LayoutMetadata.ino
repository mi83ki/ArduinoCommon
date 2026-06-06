/**
 * LayoutMetadata - EEPROMLayoutStore と EEPROMSession の使用例
 *
 * 既存EEPROMレイアウトのメタ情報を固定アドレスに保持し、
 * 構造体本体は EEPROMStore<T> で保存します。
 */

#include <stddef.h>
#include <string.h>

#include <EEPROMStore.h>

struct LayoutHeader {
  uint16_t version;
  uint16_t configSize;
  uint16_t configAddress;
  char itemName[4];
};

struct DeviceConfig {
  uint8_t brightness;
  bool enabled;
  char deviceName[16];
};

constexpr uint16_t kEepromSize = 512;
constexpr uint16_t kHeaderAddress = 10;
constexpr uint16_t kConfigAddress = 32;
constexpr uint16_t kLayoutVersion = 2;

EEPROMSession session(kEepromSize);
EEPROMLayoutStore<LayoutHeader> layoutStore(session, kHeaderAddress);

DeviceConfig defaults = {
    64,
    true,
    "layout-demo",
};

LayoutHeader expectedHeader = {
    kLayoutVersion,
    sizeof(DeviceConfig),
    kConfigAddress,
    "CFG",
};

bool isSameLayout(const LayoutHeader& actual, const LayoutHeader& expected) {
  const EEPROMStoreUtil::SectionInfo expectedSection = {
      expected.configSize,
      expected.configAddress,
  };
  const EEPROMStoreUtil::SectionInfo actualSection = {
      actual.configSize,
      actual.configAddress,
  };

  return actual.version == expected.version &&
         EEPROMStoreUtil::isSectionCompatible(expectedSection, actualSection) &&
         strncmp(actual.itemName, expected.itemName, sizeof(expected.itemName)) ==
             0;
}

void printHeader(const LayoutHeader& header) {
  Serial.println(F("\n--- Layout Header ---"));
  Serial.print(F("  version : "));
  Serial.println(header.version);
  Serial.print(F("  size    : "));
  Serial.println(header.configSize);
  Serial.print(F("  address : "));
  Serial.println(header.configAddress);
  Serial.print(F("  item    : "));
  Serial.println(header.itemName);
}

void printConfig() {
  Serial.println(F("\n--- Device Config ---"));
  Serial.print(F("  brightness : "));
  Serial.println(defaults.brightness);
  Serial.print(F("  enabled    : "));
  Serial.println(defaults.enabled ? F("true") : F("false"));
  Serial.print(F("  name       : "));
  Serial.println(defaults.deviceName);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
  }

  session.begin();
  DeviceConfig loadedConfig = {};
  LayoutHeader header = layoutStore.read();
  session.get(kConfigAddress, loadedConfig);

  if (!isSameLayout(header, expectedHeader)) {
    layoutStore.write(expectedHeader);
    session.put(kConfigAddress, defaults);
    session.commit();
    header = expectedHeader;
    loadedConfig = defaults;
    Serial.println(F("Layout header updated"));
  }

  defaults = loadedConfig;
  Serial.println(F("Layout and config loaded"));
  printHeader(header);
  printConfig();

  Serial.println(F("\n--- EEPROM Dump ---"));
  session.dump([](const String& line) { Serial.println(line); }, 0, 64);
  session.end();
}

void loop() {}
