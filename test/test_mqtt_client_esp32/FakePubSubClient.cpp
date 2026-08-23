#include "PubSubClient.h"

namespace FakePubSubClientState {

std::vector<ServerCall> serverCalls;
std::vector<std::string> subscribeCalls;
uint32_t disconnectCalls = 0;
uint32_t connectCalls = 0;
uint32_t loopCalls = 0;
bool connected = false;
bool connectResult = false;

void reset() {
  fakeMillis = 0;
  serverCalls.clear();
  subscribeCalls.clear();
  disconnectCalls = 0;
  connectCalls = 0;
  loopCalls = 0;
  connected = false;
  connectResult = false;
}

}  // namespace FakePubSubClientState

PubSubClient::PubSubClient(WiFiClient&) {}

PubSubClient& PubSubClient::setServer(const char* host, uint16_t port) {
  FakePubSubClientState::serverCalls.push_back(
      {host == nullptr ? "" : host, port});
  return *this;
}

PubSubClient& PubSubClient::setCallback(
    std::function<void(char*, uint8_t*, unsigned int)> callback) {
  _callback = callback;
  return *this;
}

PubSubClient& PubSubClient::setBufferSize(uint16_t size) {
  _bufferSize = size;
  return *this;
}

uint16_t PubSubClient::getBufferSize() const { return _bufferSize; }

bool PubSubClient::connect(const char*) {
  ++FakePubSubClientState::connectCalls;
  FakePubSubClientState::connected = FakePubSubClientState::connectResult;
  return FakePubSubClientState::connected;
}

bool PubSubClient::connected() const {
  return FakePubSubClientState::connected;
}

void PubSubClient::disconnect() {
  ++FakePubSubClientState::disconnectCalls;
  FakePubSubClientState::connected = false;
}

bool PubSubClient::loop() {
  ++FakePubSubClientState::loopCalls;
  return true;
}

bool PubSubClient::subscribe(const char* topic) {
  FakePubSubClientState::subscribeCalls.push_back(
      topic == nullptr ? "" : topic);
  return FakePubSubClientState::connected;
}

bool PubSubClient::beginPublish(const char*, unsigned int, bool) { return true; }

size_t PubSubClient::print(const char*) { return 0; }

bool PubSubClient::endPublish() { return true; }

bool PubSubClient::publish(const char*, const uint8_t*, unsigned int, bool) {
  return FakePubSubClientState::connected;
}
