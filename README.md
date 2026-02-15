# ArduinoCommon

Arduino common libraries

---

## Timer

例えばメインループで特定の周期で実行したい処理がある場合に使用する。

[Timer使用例](examples/Timer/Timer.ino)

---

## ServoESP32

ESP32マイコンでRCサーボモータを駆動するときに使用する。

- -90°～90°の角度を指定可能
- 回転角速度[°/s]を指定可能

[ServoESP32使用例](examples/ServoESP32/ServoESP32.ino)

---

## WiFiESP32

ESP32マイコンでWiFiを使用するライブラリ。再接続処理などを実装。

[WiFiESP32使用例](examples/WiFiESP32/WiFiESP32.ino)

---

## EEPROMStore

任意の構造体をEEPROMに安全に読み書きするArduinoライブラリ。

### 特徴

- **テンプレート対応** — 任意の構造体をそのまま保存・読み込み
- **CRC16整合性チェック** — データ破損を検知し、デフォルト値で自動復旧
- **マジックナンバー** — 未初期化EEPROMとの区別、複数ストアの識別
- **差分書き込み** — AVRでは `EEPROM.put()` 内部の `update()` によりバイト単位で差分書き込み
- **マルチプラットフォーム** — AVR / ESP32 / ESP8266 対応

### 基本的な使い方

```cpp
#include <EEPROMStore.h>

struct Config {
    int   interval;
    float threshold;
    char  name[16];
};

Config defaults = { 1000, 25.5f, "sensor01" };
EEPROMStore<Config> store(0, defaults);

void setup() {
    store.begin();              // 読み込み（初回はデフォルトで初期化）
    store.data.threshold = 30;  // 値を変更
    store.save();               // 変更があれば書き込み
}
```

### API

| メソッド | 説明 |
| --- | --- |
| `EEPROMStore<T>(address, defaults, magic)` | コンストラクタ。`magic` は省略可（デフォルト `0xBEEF`） |
| `bool begin()` | 読み込み。失敗時はデフォルトで初期化。戻り値: 既存データの有無 |
| `bool save()` | CRC比較で変更時のみ書き込み。戻り値: 書き込みの有無 |
| `void forceSave()` | 比較なしで強制書き込み |
| `void reset()` | デフォルト値に戻して書き込み |
| `static uint16_t requiredSize()` | ストアが使用するバイト数 |
| `uint16_t nextAddress()` | 次のストアの開始アドレス |
| `T data` | 読み書き対象の構造体（直接アクセス） |

### 複数ストアの配置

```cpp
EEPROMStore<SensorConfig>  sensor(0, sensorDefaults, 0xAA01);
EEPROMStore<NetworkConfig> network(sensor.nextAddress(), networkDefaults, 0xAA02);
```

### 対応アーキテクチャ

AVR, ESP32, ESP8266, その他 EEPROM.h 互換ボード

---

## ライセンス

MIT
