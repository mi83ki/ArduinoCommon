#pragma once

#include <Arduino.h>
#include <WiFi.h>

class UDPClientESP32 {
 public:
  UDPClientESP32(const char *serverIP, uint16_t serverPort,
                 uint16_t clientPort);
  ~UDPClientESP32();

  bool begin(void);
  void connectedAction(void);
  void disconnectedAction(void);
  bool send(const uint8_t *data, size_t size);
  bool sendUDP(uint8_t *data, uint8_t size);

 private:
  WiFiUDP *_udp;
  uint8_t _udpFlag;
  const char *_serverIP;
  uint16_t _serverPort;
  uint16_t _clientPort;
};
