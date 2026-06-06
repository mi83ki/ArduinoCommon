#include <Arduino.h>

#include <Log.h>
#include <TCPClientESP32.h>
#include <WiFiESP32.h>

const char *SSID = "Your WiFi SSID";
const char *PASS = "Your WiFi Password";
const char *SERVER_IP = "192.168.0.10";
const uint16_t SERVER_PORT = 9000;
const uint32_t SEND_INTERVAL_MS = 2000;

WiFiESP32 wifi = WiFiESP32(SSID, PASS);
TCPClientESP32 tcp = TCPClientESP32(SERVER_IP, SERVER_PORT);

uint32_t lastSendTime = 0;

void setup()
{
  logger.info("Start example of TCPClientESP32");
  delay(3000);
}

void loop()
{
  if (wifi.healthCheck())
  {
    tcp.connectedAction();

    if (millis() - lastSendTime >= SEND_INTERVAL_MS)
    {
      lastSendTime = millis();
      tcp.sendString("hello from TCPClientESP32");
    }

    if (tcp.isReceived())
    {
      logger.info("TCP Received: " + tcp.readString());
    }
  }
  else
  {
    tcp.disconnectedAction();
  }

  delay(100);
}
