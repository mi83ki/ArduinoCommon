#include <Speaker.h>

Speaker speaker(80);

const Buzzer::Note melody[] = {
    {72, 100},
    {76, 100},
    {79, 200},
};

void setup() {
  speaker.begin();
  speaker.playMelody(melody);
}

void loop() {
  speaker.updateWav();

  if (!speaker.update()) {
    delay(1000);
    speaker.playMelody(melody);
  }
}
