# ArduinoCommon

Arduino 向けの共通ライブラリ集です。  
ESP32 を中心に、複数プロジェクトで再利用する部品をまとめています。

## 含まれるライブラリ

### Timer

一定周期の判定や経過時間の取得を行うユーティリティです。

- 周期到達判定
- 開始からの経過時間取得
- グローバル時刻取得

使用例: `examples/Timer/Timer.ino`

### ServoESP32

ESP32 の LEDC を使って RC サーボを制御します。

- 角度指定
- 角度ごとの補正を考慮した移動量計算

使用例: `examples/ServoESP32/ServoESP32.ino`

### WiFiESP32

ESP32 の Wi-Fi 接続を扱う補助クラスです。接続や状態確認を簡潔に扱えます。

使用例: `examples/WiFiESP32/WiFiESP32.ino`

### EEPROMStore

構造体データを EEPROM に安全に保存するテンプレートクラスです。

- デフォルト値からの初期化
- CRC16 による整合性確認
- 更新時のみ書き込み
- 複数ストアの独立運用

使用例: `examples/EEPROMStore/BasicUsage/BasicUsage.ino`

## 音と振動の制御

### TimedPatternPlayer

時間付きの出力シーケンスをノンブロッキングで再生する共通基盤です。  
`Buzzer` と `Vibrator` はこのクラスを利用しており、`update()` を定期的に呼ぶだけで再生できます。

### Buzzer

MIDI ノート番号ベースでブザーを制御します。

- 単音再生: `playTone()`
- 停止: `mute()`
- メロディ再生: `playMelody()`
- 再生更新: `update()`

```cpp
#include <Buzzer.h>

Buzzer buzzer;

const Buzzer::Note melody[] = {
    {72, 100},
    {Buzzer::REST, 50},
    {76, 100},
};

void setup() {
  buzzer.begin();
  buzzer.playMelody(melody);
}

void loop() {
  buzzer.update();
}
```

使用例: `examples/Buzzer/Buzzer.ino`

### Vibrator

振動強度[%] と再生時間[ms] の組み合わせで振動を制御します。

- 全力駆動: `on()`
- 強度指定: `setPower()`
- 停止: `off()`
- パターン再生: `playPattern()`
- 再生更新: `update()`

```cpp
#include <Vibrator.h>

Vibrator vibrator(12, 3);

const Vibrator::PowerStep pattern[] = {
    {100, 80},
    {0, 40},
    {60, 160},
};

void setup() {
  vibrator.begin();
  vibrator.playPattern(pattern);
}

void loop() {
  vibrator.update();
}
```

使用例: `examples/Vibrator/Vibrator.ino`

### Speaker

`Speaker` は `Buzzer` を拡張し、トーン再生に加えて DAC を使った WAV 再生にも対応します。

- ブザー互換のメロディ再生
- 音量設定: `setVolume()`
- WAV 再生要求: `requestWav()`
- WAV 再生更新: `updateWav()`

```cpp
#include <Speaker.h>

Speaker speaker(80);

const Buzzer::Note melody[] = {
    {72, 100},
    {76, 100},
    {79, 200},
};

void setup() {
  speaker.begin();
  speaker.playMelody(melody);
}

void loop() {
  speaker.updateWav();
  speaker.update();
}
```

使用例: `examples/Speaker/Speaker.ino`

## NeoPixel 制御

### NeoPixelArrayBase

FastLED を使った NeoPixel 配列制御の共通基底クラスです。

- 全体塗りつぶし: `fillAll()`
- 範囲塗りつぶし: `fill()`
- 単一 LED 設定: `setPixelColor()`
- 変更時のみ更新: `update()`
- レインボー表示: `rainbow()`
- 色ユーティリティ: `setBrightness()`, `setFullBrightness()`, `getRGB()`, `getComplementaryColor()`

### NeoPixelArray

`NeoPixelArrayBase` を継承したテンプレートクラスです。  
データピン、LED タイプ、色順をテンプレート引数で指定します。

```cpp
#include <FastLED.h>
#include <NeoPixelArray.h>

constexpr uint8_t DATA_PIN = 13;
constexpr uint16_t NUM_LEDS = 16;
using Strip = NeoPixelArray<DATA_PIN, WS2812B, GRB>;

Strip strip(NUM_LEDS, 64);

void setup() {
  strip.fillAll(NeoPixelArrayBase::getRGB(0x00FF80, 100));
  strip.update();
}

void loop() {
  strip.rainbow(30, NUM_LEDS);
  strip.update();
}
```

使用例: `examples/NeoPixelArray/NeoPixelArray.ino`

`NeoPixelArrayBase` / `NeoPixelArray` は汎用部分のみを持ちます。  
LED 配置に依存するメソッドやデバイス固有のレイアウト処理は、各プロジェクト側で派生クラスやラッパーとして実装してください。

## 対応環境

- AVR
- ESP32
- ESP8266

ライブラリごとに対応範囲は異なります。  
`Buzzer` / `Vibrator` / `Speaker` / `NeoPixelArrayBase` / `NeoPixelArray` は ESP32 での利用を前提にしています。

## ライセンス

MIT
