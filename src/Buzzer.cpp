/**
 * @file Buzzer.cpp
 * @brief MIDI ノート番号ベースでブザーを制御するクラスの実装
 */

#include "Buzzer.h"

/**
 * @brief MIDI ノート番号を周波数へ変換するテーブル
 */
const double Buzzer::MIDI_TO_FREQ[] = {
    8.2,     8.7,    9.2,    9.7,    10.3,   10.9,   11.6,   12.2,    13,
    13.8,    14.6,   15.4,   16.4,   17.3,   18.4,   19.4,   20.6,    21.8,
    23.1,    24.5,   26,     27.5,   29.1,   30.9,   32.7,   34.6,    36.7,
    38.9,    41.2,   43.7,   46.2,   49,     51.9,   55,     58.3,    61.7,
    65.4,    69.3,   73.4,   77.8,   82.4,   87.3,   92.5,   98,      103.8,
    110,     116.5,  123.5,  130.8,  138.6,  146.8,  155.6,  164.8,   174.6,
    185,     196,    207.7,  220,    233.1,  246.9,  261.6,  277.2,   293.7,
    311.1,   329.6,  349.2,  370,    392,    415.3,  440,    466.2,   493.9,
    523.3,   554.4,  587.3,  622.3,  659.3,  698.5,  740,    784,     830.6,
    880,     932.3,  987.8,  1046.5, 1108.7, 1174.7, 1244.5, 1318.5,  1396.9,
    1480,    1568,   1661.2, 1760,   1864.7, 1975.5, 2093,   2217.5,  2349.3,
    2489,    2637,   2793.8, 2960,   3136,   3322.4, 3520,   3729.3,  3951.1,
    4186,    4434.9, 4698.6, 4978,   5274,   5587.7, 5919.9, 6271.9,  6644.9,
    7040,    7458.6, 7902.1, 8372,   8869.8, 9397.3, 9956.1, 10548.1, 11175.3,
    11839.8, 12543.9};

Buzzer::Buzzer(uint8_t pin, uint8_t channel, uint16_t baseFrequency,
               uint8_t resolutionBits)
    : _pin(pin),
      _channel(channel),
      _baseFrequency(baseFrequency),
      _resolutionBits(resolutionBits),
      _begun(false) {}

Buzzer::~Buzzer() {}

/**
 * @brief LEDC と出力ピンを初期化する
 */
void Buzzer::begin() {
  if (_begun) {
    return;
  }

  ledcSetup(_channel, _baseFrequency, _resolutionBits);
  attach();
  _begun = true;
  mute();
}

/**
 * @brief 単音を再生する
 * @param midiNote MIDI ノート番号。`REST` の場合は無音
 */
void Buzzer::playTone(uint8_t midiNote) {
  begin();

  if (midiNote == REST) {
    mute();
  } else if (midiNote >= MIDI_NOTE_MIN && midiNote <= MIDI_NOTE_MAX) {
    ledcWriteTone(_channel, MIDI_TO_FREQ[midiNote]);
  }
}

/**
 * @brief ブザー出力を停止する
 */
void Buzzer::mute() {
  begin();
  ledcWriteTone(_channel, 0.0);
}

/**
 * @brief メロディ再生を開始する
 * @param melody メロディ配列
 * @param length 配列長
 */
void Buzzer::playMelody(const Note* melody, size_t length) {
  play(melody, length);
}

void Buzzer::playMelody(Melody melody) {
  play(melody);
}

/**
 * @brief メロディ再生状態を更新する
 * @return true 再生中
 * @return false 停止中または再生完了
 */
bool Buzzer::update() {
  begin();
  return TimedPatternPlayer::update();
}

Buzzer::Melody Buzzer::captureMelody() const {
  return capturePlayback();
}

/**
 * @brief 現在のステップ値をブザー出力へ反映する
 * @param output MIDI ノート番号
 */
void Buzzer::applyOutput(uint8_t output) {
  playTone(output);
}

void Buzzer::stopOutput() {
  mute();
}

/**
 * @brief ブザー出力ピンを LEDC チャンネルへ接続する
 */
void Buzzer::attach() {
  ledcAttachPin(_pin, _channel);
}

/**
 * @brief ブザー出力ピンを LEDC チャンネルから切り離す
 */
void Buzzer::detach() {
  ledcDetachPin(_pin);
}
