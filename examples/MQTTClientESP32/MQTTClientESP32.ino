#include <Arduino.h>

#include <Log.h>
#include <Timer.h>
#include <WiFiESP32.h>
#include <MQTTClientESP32.h>


const char *SSID = "Your WiFi SSID";
const char *PASS = "Your WiFi Password";
const char *MQTT_HOST = "XX.XX.XX.XX";
const int MQTT_PORT = 1883;
const int MQTT_BUFFER_SIZE = 256;

const String PUB_TOPIC = "sample/topic";

WiFiESP32 wifi = WiFiESP32(SSID, PASS);
MQTTClientESP32 *mqttClient;

void onMessage(String topic, String payload)
{
  logger.info("Received message: " + topic + ", payload: " + payload);
}

void setup()
{
  logger.info("Start example of MQTTClientESP32");

  // WiFi接続
  if (!wifi.begin()) {
    logger.info("WiFi Init Fail");
    ESP.restart();
  }
  // MQTT接続
  mqttClient = new MQTTClientESP32(MQTT_HOST, MQTT_PORT, MQTT_BUFFER_SIZE);
  // サブスクライブ設定
  mqttClient->subscribe(PUB_TOPIC);
  mqttClient->registOnMessageCallback(onMessage);
  delay(3000);
}

void loop()
{
  static Timer timer(1000);
  if (wifi.healthCheck() && mqttClient->healthCheck())
  {
    if (timer.isCycleTime()) {
      String payload = "{\"time\":" + String(millis()) + "}";
      mqttClient->publish(PUB_TOPIC, payload);
      logger.debug("send topic: " + PUB_TOPIC + ", payload: " + payload);
    }
  }
  delay(1);
}
