#include <Arduino.h>

#include <Log.h>
#include <MQTTClientESP32.h>


const char *SSID = "Your WiFi SSID";
const char *PASS = "Your WiFi Password";
WiFiESP32 wifi = WiFiESP32(SSID, PASS);
MQTTClientESP32 *mqttClient;

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
  delay(3000);
}

void loop()
{
  if (wifi.healthCheck() && mqttClient->healthCheck())
  {
    String payload = "{\"time\":" + String(millis()) + "}";
    mqttClient->publish("sample/topic", payload);
    logger.debug("send topic: " + pubTopic + ", payload: " + payload);
  }
  delay(1000);
}
