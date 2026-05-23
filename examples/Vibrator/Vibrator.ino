#include <Vibrator.h>

Vibrator vibrator(12, 3);

const Vibrator::PowerStep pattern[] = {
    {100, 80},
    {0, 40},
    {60, 160},
};

void setup() {
  vibrator.begin();
  vibrator.playPattern(pattern);
}

void loop() {
  if (!vibrator.update()) {
    delay(1000);
    vibrator.playPattern(pattern);
  }
}
