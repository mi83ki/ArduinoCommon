#include <FastLED.h>
#include <NeoPixelArray.h>

constexpr uint8_t DATA_PIN = 13;
constexpr uint16_t NUM_LEDS = 16;
using Strip = NeoPixelArray<DATA_PIN, WS2812B, GRB>;

Strip strip(NUM_LEDS, 64);

void setup() {
  strip.fillAll(NeoPixelArrayBase::getRGB(0x00FF80, 100));
  strip.update();
}

void loop() {
  strip.rainbow(30, NUM_LEDS);
  strip.update();
}
