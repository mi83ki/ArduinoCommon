#pragma once
#include <Arduino.h>
#include <string>

struct TcpFake {
  bool connectResult{true};
  unsigned connects{0}, stops{0}, reads{0};
  std::string input, output;
  size_t writeCount{0};
  char delimiter{0};
};
extern TcpFake tcpFake;
class WiFiClient {
 public:
  int connect(const char*, uint16_t) {
    ++tcpFake.connects;
    return tcpFake.connectResult;
  }
  void stop() { ++tcpFake.stops; }
  int available() { return tcpFake.input.size(); }
  size_t print(String text) {
    tcpFake.output = text.c_str();
    return tcpFake.writeCount;
  }
  String readStringUntil(char delimiter) {
    ++tcpFake.reads;
    tcpFake.delimiter = delimiter;
    const auto position = tcpFake.input.find(delimiter);
    const auto result = tcpFake.input.substr(0, position);
    tcpFake.input.erase(0, position == std::string::npos ? tcpFake.input.size() : position + 1);
    return String(result);
  }
};
