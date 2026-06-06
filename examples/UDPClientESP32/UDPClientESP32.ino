#include <Arduino.h>

#include <Log.h>
#include <UDPClientESP32.h>
#include <WiFiESP32.h>

const char *SSID = "Your WiFi SSID";
const char *PASS = "Your WiFi Password";
const char *SERVER_IP = "192.168.0.10";
const uint16_t SERVER_PORT = 9000;
const uint16_t CLIENT_PORT = 9001;
const uint32_t SEND_INTERVAL_MS = 2000;

WiFiESP32 wifi = WiFiESP32(SSID, PASS);
UDPClientESP32 udp = UDPClientESP32(SERVER_IP, SERVER_PORT, CLIENT_PORT);

uint32_t lastSendTime = 0;

void setup()
{
  logger.info("Start example of UDPClientESP32");
  delay(3000);
}

void loop()
{
  if (wifi.healthCheck())
  {
    udp.connectedAction();

    if (millis() - lastSendTime >= SEND_INTERVAL_MS)
    {
      lastSendTime = millis();
      const uint8_t message[] = "hello from UDPClientESP32";
      udp.send(message, sizeof(message) - 1);
    }
  }
  else
  {
    udp.disconnectedAction();
  }

  delay(100);
}
