#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "TimedPatternPlayer.h"

class Vibrator : public TimedPatternPlayer {
 public:
  using PowerStep = TimedPatternPlayer::Step;
  using Pattern = TimedPatternPlayer::Pattern;

  static const uint8_t DEFAULT_RESOLUTION_BITS = 8;
  static constexpr double DEFAULT_FREQUENCY = 1000.0;

  Vibrator(uint8_t pin, uint8_t channel,
           double frequency = DEFAULT_FREQUENCY,
           uint8_t resolutionBits = DEFAULT_RESOLUTION_BITS);
  virtual ~Vibrator();

  void begin();
  void on();
  void setPower(uint8_t percent);
  void off();
  bool isOn() const;
  void playPattern(const PowerStep* pattern, size_t length);
  void playPattern(Pattern pattern);
  template <size_t N>
  void playPattern(const PowerStep (&pattern)[N]) {
    playPattern(pattern, N);
  }
  bool update() override;

 protected:
  void applyOutput(uint8_t output) override;
  void stopOutput() override;

 private:
  void enablePWM();
  void disablePWM();

  uint8_t _pin;
  uint8_t _channel;
  double _frequency;
  uint8_t _resolutionBits;
  uint32_t _pwmResolution;
  bool _pwmEnabled;
  bool _begun;
};
