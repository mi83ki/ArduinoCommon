# ArduinoCommon

Arduino 向けの共通ライブラリ集です。  
ESP32 を中心に、複数プロジェクトで再利用する部品をまとめています。

---

## 含まれるライブラリ

### Timer

一定周期の判定や経過時間の計測を行うユーティリティです。

- 周期実行判定
- 開始からの経過時間取得
- グローバル時刻取得

使用例: `examples/Timer/Timer.ino`

### ServoESP32

ESP32 の LEDC を使って RC サーボを制御します。

- 角度指定
- 角速度を考慮した追従制御

使用例: `examples/ServoESP32/ServoESP32.ino`

### WiFiESP32

ESP32 の Wi-Fi 接続を扱う補助クラスです。接続や状態確認を簡潔に扱えます。

使用例: `examples/WiFiESP32/WiFiESP32.ino`

### EEPROMStore

構造体データを EEPROM に保存・復元するためのテンプレートクラスです。

- デフォルト値からの初期化
- CRC16 による破損検出
- 更新時のみ書き込み
- 複数ストアの連結利用

使用例: `examples/EEPROMStore/BasicUsage/BasicUsage.ino`

---

## 音と振動の出力

### TimedPatternPlayer

時間付きの出力シーケンスをノンブロッキングで再生する共通基底クラスです。  
`Buzzer` と `Vibrator` はこのクラスを利用しており、どちらも `update()` を周期的に呼ぶだけで再生できます。

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

出力強度[%] と再生時間[ms] の組み合わせで振動を制御します。

- 全開出力: `on()`
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

`Speaker` は `Buzzer` を継承し、トーン再生に加えて DAC を使った WAV 再生にも対応します。

- ブザー互換のメロディ再生
- 音量設定: `setVolume()`
- WAV 再生予約: `requestWav()`
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

---

## 対応環境

- AVR
- ESP32
- ESP8266

ライブラリごとに利用できる機能は異なります。`Buzzer` / `Vibrator` / `Speaker` は ESP32 向け実装です。

---

## ライセンス

MIT
