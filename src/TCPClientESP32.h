#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "Timer.h"

class TCPClientESP32 {
 public:
  TCPClientESP32(const char *serverIP, uint16_t serverPort,
                 char endCharacter = '\n',
                 uint32_t reconnectIntervalMs = 5000);
  ~TCPClientESP32();

  bool begin(void);
  void connectedAction(void);
  void disconnectedAction(void);
  bool sendString(String str);
  uint16_t available(void);
  uint16_t isReceived(void);
  uint16_t isRecieved(void);
  String readString(void);

 private:
  WiFiClient *_tcp;
  uint8_t _tcpFlag;
  const char *_serverIP;
  uint16_t _serverPort;
  char _endCharacter;
  uint32_t _reconnectIntervalMs;
  Timer _timer;
};
