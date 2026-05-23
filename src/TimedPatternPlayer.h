#pragma once

#include <Arduino.h>
#include <stddef.h>

class TimedPatternPlayer {
 public:
  struct Step {
    uint8_t output;
    uint16_t durationMs;
  };

  struct Pattern {
    const Step* steps;
    size_t length;
  };

  TimedPatternPlayer();
  virtual ~TimedPatternPlayer();

  void play(const Step* steps, size_t length);
  void play(Pattern pattern);
  void stop();
  virtual bool update();
  bool isPlaying() const;
  bool isFinished() const;
  Pattern capturePlayback() const;

 protected:
  virtual void applyOutput(uint8_t output) = 0;
  virtual void stopOutput() = 0;

 private:
  void startCurrentStep();
  void finish();

  const Step* _steps;
  size_t _length;
  size_t _index;
  uint32_t _stepStartedAt;
  bool _playing;
  bool _finished;
};
