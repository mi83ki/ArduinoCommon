#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Arduino.h"
#include "WiFi.h"

#define MQTT_MAX_HEADER_SIZE 5

namespace FakePubSubClientState {

struct ServerCall {
  std::string host;
  uint16_t port;
};

extern std::vector<ServerCall> serverCalls;
extern std::vector<std::string> subscribeCalls;
extern uint32_t disconnectCalls;
extern uint32_t connectCalls;
extern uint32_t loopCalls;
extern bool connected;
extern bool connectResult;

void reset();

}  // namespace FakePubSubClientState

class PubSubClient {
 public:
  explicit PubSubClient(WiFiClient& client);
  PubSubClient& setServer(const char* host, uint16_t port);
  PubSubClient& setCallback(
      std::function<void(char*, uint8_t*, unsigned int)> callback);
  PubSubClient& setBufferSize(uint16_t size);
  uint16_t getBufferSize() const;
  bool connect(const char* clientId);
  bool connected() const;
  void disconnect();
  bool loop();
  bool subscribe(const char* topic);
  bool beginPublish(const char* topic, unsigned int length, bool retained);
  size_t print(const char* payload);
  bool endPublish();
  bool publish(const char* topic, const uint8_t* payload,
               unsigned int length, bool retained);

 private:
  uint16_t _bufferSize = 256;
  std::function<void(char*, uint8_t*, unsigned int)> _callback;
};
