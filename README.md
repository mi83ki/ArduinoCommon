# ArduinoCommon

Arduino / ESP32 向けの共通ライブラリ集です。
複数のプロジェクトで再利用することを前提に、入出力、永続化、音、LED、通信まわりの小さな部品をまとめています。

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

ESP32のWi-Fi接続を管理する補助クラスです。通常接続先へ直接接続し、失敗時は登録済みフォールバック候補から接続先を選択します。

```cpp
#include <WiFiESP32.h>

WiFiESP32 wifi("primary-ssid", "primary-password");

void setup() {
  wifi.addAP("fallback-ssid-1", "fallback-password-1");
  wifi.addAP("fallback-ssid-2", "fallback-password-2", "192.168.2.50",
             "192.168.2.1", "255.255.255.0");

  // 通常接続先の固定IP。
  wifi.setStaticIp("192.168.1.50", "192.168.1.1", "255.255.255.0");
  wifi.begin();
}

void loop() {
  wifi.healthCheck();
  delay(1000);
}
```

- 2引数の`addAP()`はDHCPを使うフォールバック候補を追加します。5引数版では候補固有の固定IP、ゲートウェイ、サブネットを指定できます。
- 全フォールバック候補がDHCPならWiFiMultiを使用します。固定IP候補が含まれる場合は1回のスキャン結果からRSSI順に直接接続します。候補の登録順は優先順位ではありません。
- `setStaticIp()`は通常接続先へ固定IPを適用します。DNSを指定する場合は `setStaticIp(ip, gateway, mask, dns1, dns2)` を使います。固定IPの予備接続先にも `addAP(ssid, password, ip, gateway, mask, dns1, dns2)` を利用できます。
- `setDhcp()`は主接続先をDHCPへ戻します。接続時には以前の固定DNSも消去します。APIへ渡した文字列は内部で所有します。
- RTCの高速接続情報はSSID・パスワード・IP・DNSの指紋が一致するときだけ再利用します。Wi-Fiを操作するタスクは一つに限定してください。
- deep sleep復帰時は前回成功したSSID、BSSID、チャンネルをRTCメモリから使い、2秒の高速接続を先に試します。パスワードはRTCメモリへ保存しません。
- 高速接続、通常接続、フォールバック接続の順で試し、すべて失敗すると`begin()`は`false`を返します。
- `healthCheck()`は切断を検出すると再接続し、失敗後は10秒間バックオフします。
- `getConnectedSsid()`で現在接続中のSSIDを取得できます。
- 接続結果のログにはSSID、IP、RSSI、所要時間を出力しますが、パスワードは出力しません。

使用例: `examples/WiFiESP32/WiFiESP32.ino`

### MQTTClientESP32

`setServer(host, port)`でMQTTブローカーを変更できます。接続先が変わった場合は現在の接続を切断し、次の`healthCheck()`で直ちに新しいブローカーへ再接続して、保存済みトピックを再購読します。同じ接続先の指定は何も行いません。

### TCPClientESP32

ESP32 の `WiFiClient` を使った TCP クライアントです。接続、再接続、終端文字付きの文字列送信、終端文字までの受信を扱います。
使用例: `examples/TCPClientESP32/TCPClientESP32.ino`

```cpp
#include <TCPClientESP32.h>

TCPClientESP32 tcp("192.168.0.10", 9000);

void setup() {
  tcp.begin();
}

void loop() {
  tcp.connectedAction();
  tcp.sendString("hello");
  if (tcp.isReceived()) {
    String message = tcp.readString();
  }
}
```

### UDPClientESP32

ESP32 の `WiFiUDP` を使った UDP クライアントです。ローカルポートの開始、停止、バイト列送信を扱います。
使用例: `examples/UDPClientESP32/UDPClientESP32.ino`

```cpp
#include <UDPClientESP32.h>

UDPClientESP32 udp("192.168.0.10", 9000, 9001);

void setup() {
  udp.begin();
}

void loop() {
  const uint8_t data[] = {0x01, 0x02};
  udp.send(data, sizeof(data));
}
```

### InfraredRemote

IRremote ライブラリを使って赤外線の受信と送信を扱う共通クラスです。

- 受信結果取得: `recieveIR()`, `getReceivedData()`, `getData()`, `getFormat()`
- 送信要求: `requestSend()`, `drive()`
- デバッグ出力: `setDebug()`, `printDebug()`

現在の送信実装は `RC6` を対象にしています。利用時は `IRremote` のプロトコル定数とビット長を指定してください。

```cpp
#include <Arduino.h>
#include <IRremote.h>
#include <InfraredRemote.h>

constexpr uint8_t IR_RECEIVE_PIN = 25;
constexpr uint8_t IR_SEND_PIN = 26;
constexpr uint8_t IR_DATA_BITS = 20;

InfraredRemote infraredRemote(IR_RECEIVE_PIN, IR_SEND_PIN, RC6, IR_DATA_BITS);

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (infraredRemote.recieveIR()) {
    InfraredRemote::ReceivedData data = infraredRemote.getReceivedData();
    Serial.println(String(data.protocol) + ", " + String(data.rawData, HEX));
  }

  infraredRemote.drive();
}
```

使用例: `examples/InfraredRemote/InfraredRemote.ino`

### EEPROMStore

EEPROM 関連の保存処理を共通化するクラス群です。

- `EEPROMStore<T>`
  - 1つの構造体を CRC16 付きで保存します
  - 初回起動時や破損時はデフォルト値で自動初期化します
  - `save()` は変更がない場合に書き込みを省略します
- `EEPROMSession`
  - `begin()` / `end()` / `commit()` と `put()` / `get()` をまとめて扱います
  - EEPROM 全体の 16 進ダンプを生成できます
- `EEPROMLayoutStore<Layout>`
  - 既存 EEPROM レイアウトのメタ情報を固定アドレスで管理します
  - フィールド単位の読み書きでレイアウト互換判定に使えます

使用例:
- `examples/EEPROMStore/BasicUsage/BasicUsage.ino`
  - 単一構造体を `EEPROMStore<T>` で保存する最小例
- `examples/EEPROMStore/MultipleStores/MultipleStores.ino`
  - `nextAddress()` を使って複数の構造体を連続配置する例
- `examples/EEPROMStore/SerialConfig/SerialConfig.ino`
  - シリアル入力で設定を変更し、EEPROM に保存する例
- `examples/EEPROMStore/LayoutMetadata/LayoutMetadata.ino`
  - `EEPROMSession` と `EEPROMLayoutStore` でレイアウト情報を管理する例

### Preferences設定保存（ESP32）

`settings/`の新しい公開型は`ArduinoCommon`名前空間にあります。既存の
`EEPROMStore`、`EEPROMStoreUtil`、`WiFiESP32`、`MQTTClientESP32`の名前とAPIは変更していません。

| 部品 | 責務 | 依存 |
| --- | --- | --- |
| `ISettingsBackend` | バイト列の保存・取得・指定キー削除 | 標準C++ |
| `RecordEnvelopeCodec` | 24バイトヘッダー、LE符号化、CRC32、schema検証 | 標準C++ |
| `AtomicRecordStore` | 1～2論理レコードの2スロット保存、rootによる一括確定 | 標準C++とbackend |
| `PreferencesBackend` | namespace別のNVS保存、読取専用アクセス、エラー区別 | Arduino-ESP32のPreferencesとNVS |

使用例: `examples/PreferencesSettings/main.cpp`。同ディレクトリのコードはM5Unifiedや
製品固有のセンサーに依存せず、テスト用のボード設定はATOM S3を使用します。

```sh
# ArduinoCommonを作業ディレクトリとして実行
pio test -e native_settings
pio run -e esp32_preferences_example
```

サンプルはUSBシリアルの`s`で1バイトの設定値を増やして保存し、`r`で読み戻します。
再起動後に値が残ることは実機で確認してください。自動テストは電源断相当の障害を模擬しますが、
実NVSの電源断試験を代替しません。保存だけのビルドは`ARDUINOCOMMON_DISABLE_PROVISIONING`を指定します。

保存するpayloadの型・既定値・移行方法は利用側が定義し、構造体のメモリーをそのまま保存しません。
`commit(expectedGeneration, updates, metadata)`は変更レコードを先に保存し、採用するrootを最後に保存します。
更新しないレコードは再書込しません。保存処理の呼出元は1つに直列化してください。

`Indeterminate`はroot保存の結果不明です。`reconcile()`で永続データを読み直し、
generationと利用側のmetadata（操作ID等）を照合してから成功・失敗を判断します。
`Corrupt`や`UnsupportedSchema`を受けても共通部品は自動初期化しません。
namespace/キーはASCIIの1～15文字、削除は指定キーのみです。全消去APIは提供しません。

Preferencesはframework同梱のものを使います。既存メタデータのEEPROM/PubSubClientや
Arduino Library Manager用のFastLED/IRremoteは他の既存部品の依存であり、
この保存APIに必要なものではありません。`architectures=*`はライブラリ全体の宣言で、
PreferencesBackendの非ESP32対応を意味しません。

共通の`native_settings`はESP32境界だけを`test/settings_mock`で置き換えます。
製品側がライブラリ全体を`lib_ignore`する場合は、PlatformIOのpre-scriptの`BuildSources`で
`RecordEnvelopeCodec.cpp`と`AtomicRecordStore.cpp`だけを追加し、利用側のbackendを渡してください。
ライブラリのtest/mock全体を利用側へincludeしないでください。

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

### 設定保存とWi-Fiプロビジョニング

`settings/`はバイト列の保存機構、`provisioning/`はAP資格情報・Wi-Fi検証・接続試験・設定画面の部品です。機種、MQTT、校正、設定の確定条件、再起動判断を含みません。保存だけなら[PreferencesSettings](examples/PreferencesSettings/)、画面を含む例は[WiFiProvisioning](examples/WiFiProvisioning/README.md)を参照してください。

- `ApCredentialStore`: backend・SSID接頭辞・MAC・安全な乱数生成を注入します。`loadOrCreate`は未保存時だけ生成し、破損・未知schema・I/O異常では既存のQRラベルを無効化する自動再生成をしません。明示的な`regenerate`の許可条件は製品側で判断します。ESP32の乱数はWi-Fi等のエントロピー源を有効にして利用します。
- `WiFiProfileValidator`: SDKなしで最大4プロファイル、SSID・認証・IP・DNS、パスワードのkeep/replace/clearを検証します。プロファイルごとの製品項目は利用側で保持・検証します。
- `WiFiProvisioningProbe`: Wi-Fiの単一所有者から`begin/start/poll/cancel/finish/stop`を呼びます。`start`の期限は`millis()`基準の絶対時刻、上限20秒です。成功後はSTAを維持し、利用側の診断・保存終了時に`finish`します。スキャンと接続試験は直列です。通常のWiFiESP32と同時実行しません。
- `ProvisioningPortalESP32`: 起動前の`addHandler`で製品APIを登録します。HTTPハンドラーは値をコピーしてキューへ渡すだけとし、Wi-Fi・NVS・停止処理はownerのloopで実施します。`requestStop`→`tick`でHTTPタスク外から停止します。`apAvailable`以外のProbe操作・結果取得もowner側に限定します。

PortalはAP宛先・Host・Origin・RAMセッションを確認し、bodyは4096バイト、総受信3秒、socketは2本に制限します。`GET /api/session`、SSID検索、`POST /api/activity`は共通で提供します。画面と入力操作のactivityだけが無操作期限の基準となり、状態ポーリングは更新しません。CNAは通常ブラウザーで固定URLを開く案内とし、自動Safari起動を前提にしません。AP/STAのサブネット重複時はAPを一時停止します。

共通JavaScriptの`WiFiForm.mount(container, config, constraints)`、`read()`、`showErrors(errors)`、`clearSecrets()`を利用します。`constraints.maximumProfiles`と`profileExtension(card, profile)`で利用側の項目を追加できます。後者は`read()`を持つオブジェクトを返します。`Client`は秘密値をURL・localStorage・ログに保存しません。独自の処理段階は`createStatus().show({phase, message})`のmessageで表します。

`PortalTypes.h`の要求・応答DTOはSDKなしでも利用できます。`setSessionHandler`を起動前に指定すると、共通のAP/Host/Origin検証後に渡されるtokenへ、製品側のrevisionや制約を追加して返せます。起動後の差替えはできません。ハンドラーでAP資格情報を返さないでください。

通常運転の`WiFiESP32`は`setReconnectInterval(ms)`で再試行間隔を指定でき、未指定は従来の10秒です。`completedConnectionCycles()`は全候補を実際に試した回数を返し、単なるバックオフ待ちは含みません。APへ移る条件やRTCの失敗回数は製品側で判断してください。この2つのAPIもWi-Fi所有タスクから利用します。

HTTP/ProbeのESP32実装はArduino core 2.0.17で検証しています。Preferences、DNSServer、WiFiはframework同梱です。保存だけの利用で`ARDUINOCOMMON_DISABLE_PROVISIONING`を定義すると、HTTP/ProbeはSDK include前に除外されます。設定名は共通の制御フラグであり製品の機種defineではありません。`esp32_preferences_gc_example`はフラグなしのリンク除去も比較します。新規部品によって全利用者へM5・ArduinoJson・MQTT依存を追加しません。

- AVR
- ESP32
- ESP8266

ライブラリごとに対応範囲は異なります。
`Buzzer` / `Vibrator` / `Speaker` / `NeoPixelArrayBase` / `NeoPixelArray` は ESP32 での利用を前提にしています。

## メタデータ管理

`library.json` を正本として扱います。`library.properties` は生成物なので、直接編集しません。

同期コマンド:

```bash
python lib/ArduinoCommon/tools/sync_library_properties.py
```

差分確認のみを行う場合:

```bash
python lib/ArduinoCommon/tools/sync_library_properties.py --check
```

更新手順:

1. `library.json` の `name` または `version` を変更
2. 同期コマンドを実行
3. 生成された `library.properties` をコミット

## ライセンス

MIT
