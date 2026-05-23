#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <driver/dac.h>

#include "Buzzer.h"

class Speaker : public Buzzer {
 public:
  static const uint8_t DEFAULT_SPEAKER_PIN = 25;
  static const uint8_t DEFAULT_SHUTDOWN_PIN = 14;

  Speaker(uint8_t volumePercent,
          uint8_t speakerPin = DEFAULT_SPEAKER_PIN,
          uint8_t shutdownPin = DEFAULT_SHUTDOWN_PIN,
          uint8_t channel = Buzzer::DEFAULT_CHANNEL);
  virtual ~Speaker();

  void begin();
  void enable();
  void disable();
  void setVolume(uint8_t percent);
  void requestWav(const unsigned char* audio, uint32_t length);
  bool updateWav();
  bool update() override;

 private:
  enum OutputMode {
    TONE_OUTPUT,
    WAV_OUTPUT
  };

  void initVolume();
  void setOutputMode(OutputMode mode);
  int playWav(const unsigned char* audio, uint32_t length);
  bool takeRequest();

  uint8_t _shutdownPin;
  const unsigned char* _audio;
  uint32_t _length;
  bool _playRequested;
  bool _volumeInitialized;
  uint8_t _initialVolume;
  bool _begun;
};
