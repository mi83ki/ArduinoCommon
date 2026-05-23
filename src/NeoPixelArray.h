#pragma once

#include <FastLED.h>

#include "NeoPixelArrayBase.h"

template <uint8_t DATA_PIN,
          template <uint8_t PIN, EOrder ORDER> class CHIPSET = WS2812B,
          EOrder COLOR_ORDER = GRB>
class NeoPixelArray : public NeoPixelArrayBase {
 public:
  NeoPixelArray(uint16_t num, uint8_t brightness = 255)
      : NeoPixelArrayBase(num) {
    FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(colors(), size());
    setBrightnessLevel(brightness);
  }

  virtual ~NeoPixelArray() {}
};
