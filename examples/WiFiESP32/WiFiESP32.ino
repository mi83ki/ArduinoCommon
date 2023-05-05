#include <Arduino.h>

#include <Log.h>
#include <WiFiESP32.h>


const char *SSID = "Your WiFi SSID";
const char *PASS = "Your WiFi Password";
WiFiESP32 wifi = WiFiESP32(SSID, PASS);

void setup()
{
  logger.info("Start example of WiFiESP32");
  delay(3000);
}

void loop()
{
  if (wifi.healthCheck())
  {
    logger.info("Do connected action.");
  }
  delay(1000);
}
