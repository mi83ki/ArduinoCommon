#include <Arduino.h>
#include <IRremote.h>

#include <InfraredRemote.h>

constexpr uint8_t IR_RECEIVE_PIN = 25;
constexpr uint8_t IR_SEND_PIN = 26;
constexpr uint8_t IR_DATA_BITS = 20;
constexpr uint32_t SAMPLE_DATA = 0xABCDE;
constexpr unsigned long SEND_INTERVAL_MS = 3000;

InfraredRemote infraredRemote(IR_RECEIVE_PIN, IR_SEND_PIN, RC6, IR_DATA_BITS);
unsigned long lastSendAt = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Start example of InfraredRemote");
}

void loop() {
  if (infraredRemote.recieveIR()) {
    InfraredRemote::ReceivedData data = infraredRemote.getReceivedData();
    Serial.print("protocol=");
    Serial.print(data.protocol);
    Serial.print(", raw=0x");
    Serial.println(data.rawData, HEX);
  }

  const unsigned long now = millis();
  if (now - lastSendAt >= SEND_INTERVAL_MS) {
    lastSendAt = now;
    infraredRemote.requestSend(SAMPLE_DATA);
  }

  infraredRemote.drive();
  delay(10);
}
