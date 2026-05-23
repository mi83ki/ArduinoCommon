#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "TimedPatternPlayer.h"

class Buzzer : public TimedPatternPlayer {
 public:
  using Note = TimedPatternPlayer::Step;
  using Melody = TimedPatternPlayer::Pattern;

  static const uint8_t REST = 0;
  static const uint8_t DEFAULT_PIN = 14;
  static const uint8_t DEFAULT_CHANNEL = 2;
  static const uint8_t DEFAULT_RESOLUTION_BITS = 13;
  static const uint16_t DEFAULT_BASE_FREQ = 5000;

  Buzzer(uint8_t pin = DEFAULT_PIN, uint8_t channel = DEFAULT_CHANNEL,
         uint16_t baseFrequency = DEFAULT_BASE_FREQ,
         uint8_t resolutionBits = DEFAULT_RESOLUTION_BITS);
  virtual ~Buzzer();

  void begin();
  void playTone(uint8_t midiNote);
  void mute();
  void playMelody(const Note* melody, size_t length);
  void playMelody(Melody melody);
  template <size_t N>
  void playMelody(const Note (&melody)[N]) {
    playMelody(melody, N);
  }
  bool update() override;
  Melody captureMelody() const;

 protected:
  void applyOutput(uint8_t output) override;
  void stopOutput() override;
  void attach();
  void detach();

 private:
  static const uint8_t MIDI_NOTE_MIN = 59;
  static const uint8_t MIDI_NOTE_MAX = 109;
  static const double MIDI_TO_FREQ[];

  uint8_t _pin;
  uint8_t _channel;
  uint16_t _baseFrequency;
  uint8_t _resolutionBits;
  bool _begun;
};
