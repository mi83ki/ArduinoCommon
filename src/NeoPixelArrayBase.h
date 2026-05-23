#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <Timer.h>

class NeoPixelArrayBase {
 public:
  explicit NeoPixelArrayBase(uint16_t num);
  virtual ~NeoPixelArrayBase();

  void fillAll(uint32_t color);
  void fill(uint32_t color, uint16_t first, uint16_t count);
  void setPixelColor(uint16_t index, uint32_t color);
  bool isChanged(void);
  virtual bool update(void);
  void rainbow(int wait, uint8_t numPixel);
  void setForceDisable(bool forceDisable);
  uint16_t size(void) const;
  uint32_t getPixelColor(uint16_t index) const;

  static uint32_t setBrightness(uint32_t inColor, uint8_t brightness);
  static uint32_t setFullBrightness(uint32_t inColor);
  static uint32_t getRGB(uint32_t inColor, uint8_t brightnessPercent);
  static uint32_t getComplementaryColor(uint32_t color);

 protected:
  void fillAll(const CRGB& color);
  void fill(const CRGB& color, uint16_t first, uint16_t count);
  void setPixelColor(uint16_t index, const CRGB& color);
  void setBrightnessLevel(uint8_t brightness);
  CRGB* colors(void);
  const CRGB* colors(void) const;

 private:
  static uint8_t red(uint32_t color);
  static uint8_t green(uint32_t color);
  static uint8_t blue(uint32_t color);
  static uint32_t toColor(uint8_t red, uint8_t green, uint8_t blue);
  static uint32_t toColor(const CRGB& color);

  uint16_t _num;
  CRGB* _colors;
  Timer _rainbowTimer;
  uint16_t _firstPixelHue;
  bool _changed;
  bool _forceDisable;
};
