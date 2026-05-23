/**
 * @file Speaker.cpp
 * @brief ブザー互換のトーン再生と DAC による WAV 再生を行うクラスの実装
 */

#include "Speaker.h"

#include <Log.h>
#include <string.h>

Speaker::Speaker(uint8_t volumePercent, uint8_t speakerPin,
                 uint8_t shutdownPin, uint8_t channel)
    : Buzzer(speakerPin, channel),
      _shutdownPin(shutdownPin),
      _audio(nullptr),
      _length(0),
      _playRequested(false),
      _volumeInitialized(false),
      _initialVolume(volumePercent),
      _begun(false) {}

Speaker::~Speaker() {}

/**
 * @brief スピーカー出力・ボリューム制御・I2C を初期化する
 */
void Speaker::begin() {
  if (_begun) {
    return;
  }

  Buzzer::begin();
  setOutputMode(TONE_OUTPUT);
  pinMode(_shutdownPin, OUTPUT);
  enable();
  Wire.begin();
  _begun = true;
}

/**
 * @brief スピーカーアンプを有効化する
 */
void Speaker::enable() {
  if (!digitalRead(_shutdownPin)) {
    digitalWrite(_shutdownPin, HIGH);
  }
}

/**
 * @brief スピーカーアンプを無効化する
 */
void Speaker::disable() {
  if (digitalRead(_shutdownPin)) {
    digitalWrite(_shutdownPin, LOW);
  }
}

/**
 * @brief デジタルボリュームへ音量を設定する
 * @param percent 音量[%]
 */
void Speaker::setVolume(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }

  uint16_t value = 127 * (uint16_t)percent / 100;
  Wire.beginTransmission(0x2F);
  Wire.write(value);
  Wire.endTransmission();

  logger.info("Speaker::setVolume(): volume = " + String(percent) +
              ", value = " + String(value));
}

/**
 * @brief WAV データ再生を予約する
 * @param audio WAV バイト列
 * @param length データ長
 */
void Speaker::requestWav(const unsigned char* audio, uint32_t length) {
  _audio = audio;
  _length = length;
  _playRequested = true;
}

/**
 * @brief 再生予約された WAV を処理する
 * @return true WAV 再生を開始した
 * @return false 再生予約なし
 */
bool Speaker::updateWav() {
  begin();
  initVolume();

  if (!takeRequest()) {
    return false;
  }

  playWav(_audio, _length);
  return true;
}

/**
 * @brief ブザー互換のメロディ再生状態を更新する
 * @return true 再生中
 * @return false 停止中または再生完了
 */
bool Speaker::update() {
  begin();
  initVolume();
  return Buzzer::update();
}

/**
 * @brief 初回のみ初期音量を設定する
 */
void Speaker::initVolume() {
  if (!_volumeInitialized) {
    _volumeInitialized = true;
    setVolume(_initialVolume);
  }
}

/**
 * @brief 出力経路をトーン再生か WAV 再生へ切り替える
 * @param mode 出力モード
 */
void Speaker::setOutputMode(OutputMode mode) {
  switch (mode) {
    case TONE_OUTPUT:
      dac_output_disable(DAC_CHANNEL_1);
      attach();
      break;
    case WAV_OUTPUT:
      detach();
      dac_output_enable(DAC_CHANNEL_1);
      break;
  }
}

/**
 * @brief WAV データを DAC へ逐次出力して再生する
 * @param audio WAV バイト列
 * @param length データ長
 * @return int 1: 正常終了, 0: 再生中断, -1: フォーマット不正
 */
int Speaker::playWav(const unsigned char* audio, uint32_t length) {
  if (audio == nullptr || length == 0) {
    return -1;
  }

  initVolume();
  setOutputMode(WAV_OUTPUT);

  if (strncmp((const char*)audio, "RIFF", 4)) {
    Serial.println("Error: It is not RIFF.");
    setOutputMode(TONE_OUTPUT);
    return -1;
  }

  if (strncmp((const char*)audio + 8, "WAVE", 4)) {
    Serial.println("Error: It is not WAVE.");
    setOutputMode(TONE_OUTPUT);
    return -1;
  }

  if (strncmp((const char*)audio + 12, "fmt ", 4)) {
    Serial.println("Error: fmt not found.");
    setOutputMode(TONE_OUTPUT);
    return -1;
  }

  uint32_t chunkSize;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint16_t bitsPerSample;
  memcpy(&chunkSize, (const char*)audio + 16, sizeof(chunkSize));
  memcpy(&numChannels, (const char*)audio + 22, sizeof(numChannels));
  memcpy(&sampleRate, (const char*)audio + 24, sizeof(sampleRate));
  memcpy(&bitsPerSample, (const char*)audio + 34, sizeof(bitsPerSample));

  uint32_t count = 16 + sizeof(chunkSize) + chunkSize;
  while (count + 8 <= length && strncmp((const char*)audio + count, "data", 4)) {
    count += 4;
    memcpy(&chunkSize, (const char*)audio + count, sizeof(chunkSize));
    count += sizeof(chunkSize) + chunkSize;
  }

  if (count + 8 > length) {
    setOutputMode(TONE_OUTPUT);
    return -1;
  }

  count += 4 + sizeof(chunkSize);
  uint16_t delayus = 1000000 / sampleRate;

  while (count < length) {
    uint8_t left;
    if (bitsPerSample == 16) {
      uint16_t data16;
      memcpy(&data16, (const char*)audio + count, sizeof(data16));
      left = ((uint16_t)data16 + 32767) >> 8;
      count += sizeof(data16);
      if (numChannels == 2) {
        count += sizeof(data16);
      }
    } else {
      uint8_t data8;
      memcpy(&data8, (const char*)audio + count, sizeof(data8));
      left = data8;
      count += sizeof(data8);
      if (numChannels == 2) {
        count += sizeof(data8);
      }
    }

    dac_output_voltage(DAC_CHANNEL_1, left);
    ets_delay_us(delayus);

    if (_playRequested) {
      setOutputMode(TONE_OUTPUT);
      return 0;
    }
  }

  dac_output_voltage(DAC_CHANNEL_1, 127);
  setOutputMode(TONE_OUTPUT);
  return 1;
}

/**
 * @brief WAV 再生予約を 1 回だけ取り出す
 * @return true 再生予約あり
 * @return false 再生予約なし
 */
bool Speaker::takeRequest() {
  if (_playRequested) {
    _playRequested = false;
    return true;
  }

  return false;
}
