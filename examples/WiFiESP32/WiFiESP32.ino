#include <Arduino.h>

#include <Log.h>
#include <WiFiESP32.h>


const char *PRIMARY_SSID = "Your primary WiFi SSID";
const char *PRIMARY_PASS = "Your primary WiFi password";
const char *FALLBACK_SSID = "Your fallback WiFi SSID";
const char *FALLBACK_PASS = "Your fallback WiFi password";
WiFiESP32 wifi = WiFiESP32(PRIMARY_SSID, PRIMARY_PASS);

void setup() {
  logger.info("Start example of WiFiESP32");
  delay(3000);

  wifi.addAP(FALLBACK_SSID, FALLBACK_PASS);
  // wifi.setStaticIp("192.168.1.50", "192.168.1.1", "255.255.255.0");
  if (!wifi.begin()) {
    logger.error("Initial WiFi connection failed.");
  }
}

void loop() {
  if (wifi.healthCheck()) {
    logger.info("Do connected action.");
  }
  delay(1000);
}
