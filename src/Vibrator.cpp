/**
 * @file Vibrator.cpp
 * @brief PWM による振動制御クラスの実装
 */

#include "Vibrator.h"

/**
 * @brief 振動モータ制御クラスを生成する
 * @param pin 出力ピン
 * @param channel LEDC チャンネル
 * @param frequency PWM 周波数
 * @param resolutionBits PWM 分解能
 */
Vibrator::Vibrator(uint8_t pin, uint8_t channel, double frequency,
                   uint8_t resolutionBits)
    : _pin(pin),
      _channel(channel),
      _frequency(frequency),
      _resolutionBits(resolutionBits),
      _pwmResolution((1UL << resolutionBits) - 1),
      _pwmEnabled(false),
      _begun(false) {}

Vibrator::~Vibrator() {}

/**
 * @brief 出力ピンと PWM を初期化する
 */
void Vibrator::begin() {
  if (_begun) {
    return;
  }

  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  ledcSetup(_channel, _frequency, _resolutionBits);
  _begun = true;
}

/**
 * @brief 全開で振動させる
 */
void Vibrator::on() {
  begin();
  disablePWM();
  digitalWrite(_pin, HIGH);
}

/**
 * @brief 振動強度を百分率で設定する
 * @param percent 0 から 100 の出力率
 */
void Vibrator::setPower(uint8_t percent) {
  begin();

  if (percent >= 100) {
    on();
    return;
  }

  if (percent == 0) {
    off();
    return;
  }

  enablePWM();
  uint32_t duty = (uint32_t)percent * _pwmResolution / 100;
  ledcWrite(_channel, duty);
}

/**
 * @brief 振動を停止する
 */
void Vibrator::off() {
  begin();
  disablePWM();
  digitalWrite(_pin, LOW);
}

/**
 * @brief 現在振動しているかを返す
 * @return true 振動中
 * @return false 停止中
 */
bool Vibrator::isOn() const {
  if (!_pwmEnabled) {
    return digitalRead(_pin) == HIGH;
  }

  return ledcRead(_channel) > 0;
}

/**
 * @brief 振動パターンの再生を開始する
 * @param pattern パターン配列
 * @param length 配列長
 */
void Vibrator::playPattern(const PowerStep* pattern, size_t length) {
  play(pattern, length);
}

void Vibrator::playPattern(Pattern pattern) {
  play(pattern);
}

/**
 * @brief 振動パターンの再生状態を更新する
 * @return true 再生中
 * @return false 停止中または再生完了
 */
bool Vibrator::update() {
  begin();
  return TimedPatternPlayer::update();
}

void Vibrator::applyOutput(uint8_t output) {
  setPower(output);
}

void Vibrator::stopOutput() {
  off();
}

/**
 * @brief PWM 出力へ切り替える
 */
void Vibrator::enablePWM() {
  if (!_pwmEnabled) {
    ledcAttachPin(_pin, _channel);
    _pwmEnabled = true;
  }
}

/**
 * @brief 通常 GPIO 出力へ戻す
 */
void Vibrator::disablePWM() {
  if (_pwmEnabled) {
    ledcDetachPin(_pin);
    _pwmEnabled = false;
  }
}
