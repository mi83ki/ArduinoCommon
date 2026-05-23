#include <Buzzer.h>

Buzzer buzzer;

const Buzzer::Note melody[] = {
    {72, 120},
    {Buzzer::REST, 80},
    {76, 120},
    {79, 240},
};

void setup() {
  buzzer.begin();
  buzzer.playMelody(melody);
}

void loop() {
  if (!buzzer.update()) {
    delay(1000);
    buzzer.playMelody(melody);
  }
}
