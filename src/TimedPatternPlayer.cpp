/**
 * @file TimedPatternPlayer.cpp
 * @brief 時間付き出力パターンをノンブロッキングで再生する基底クラスの実装
 */

#include "TimedPatternPlayer.h"

/**
 * @brief パターン未設定・停止状態で初期化する
 */
TimedPatternPlayer::TimedPatternPlayer()
    : _steps(nullptr),
      _length(0),
      _index(0),
      _stepStartedAt(0),
      _playing(false),
      _finished(true) {}

TimedPatternPlayer::~TimedPatternPlayer() {}

/**
 * @brief 指定したステップ配列の再生を開始する
 * @param steps 出力ステップ配列
 * @param length 配列長
 */
void TimedPatternPlayer::play(const Step* steps, size_t length) {
  stop();

  if (steps == nullptr || length == 0) {
    _finished = true;
    return;
  }

  _steps = steps;
  _length = length;
  _index = 0;
  _playing = true;
  _finished = false;
  startCurrentStep();
}

void TimedPatternPlayer::play(Pattern pattern) {
  play(pattern.steps, pattern.length);
}

/**
 * @brief 再生を停止して内部状態を初期化する
 */
void TimedPatternPlayer::stop() {
  if (_playing) {
    stopOutput();
  }

  _steps = nullptr;
  _length = 0;
  _index = 0;
  _playing = false;
  _finished = true;
}

/**
 * @brief 現在のステップ再生状態を進める
 * @return true 再生中
 * @return false 停止中または再生完了
 */
bool TimedPatternPlayer::update() {
  if (!_playing || _steps == nullptr) {
    return false;
  }

  if (millis() - _stepStartedAt < _steps[_index].durationMs) {
    return true;
  }

  _index++;
  if (_index >= _length) {
    finish();
    return false;
  }

  startCurrentStep();
  return true;
}

bool TimedPatternPlayer::isPlaying() const {
  return _playing;
}

bool TimedPatternPlayer::isFinished() const {
  return _finished;
}

/**
 * @brief 現在位置から再生中パターンを取り出す
 * @return Pattern 現在の再生位置以降のパターン情報
 */
TimedPatternPlayer::Pattern TimedPatternPlayer::capturePlayback() const {
  if (!_playing || _steps == nullptr || _index >= _length) {
    return {nullptr, 0};
  }

  return {_steps + _index, _length - _index};
}

void TimedPatternPlayer::startCurrentStep() {
  _stepStartedAt = millis();
  applyOutput(_steps[_index].output);
}

/**
 * @brief 出力を停止して再生完了状態へ遷移する
 */
void TimedPatternPlayer::finish() {
  stopOutput();
  _steps = nullptr;
  _length = 0;
  _index = 0;
  _playing = false;
  _finished = true;
}
