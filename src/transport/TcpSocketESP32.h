#pragma once
#include <array>
#include "ByteStream.h"

namespace ArduinoCommon {
namespace Transport {
class TcpSocketESP32 final : public ByteStream {
 public:
  TcpSocketESP32() = default;
  ~TcpSocketESP32() override;
  TcpSocketESP32(const TcpSocketESP32&) = delete;
  TcpSocketESP32& operator=(const TcpSocketESP32&) = delete;
  bool connect(const std::array<uint8_t, 4>& address, uint16_t port,
               uint32_t timeoutMs = 1000);
  IoResult read(uint8_t* output, std::size_t capacity) override;
  IoResult write(const uint8_t* data, std::size_t size) override;
  void close() override;
 private:
  int _socket{-1};
};
class ArduinoMillisClock final : public Clock {
 public:
  uint32_t nowMs() const override;
};
}
}
