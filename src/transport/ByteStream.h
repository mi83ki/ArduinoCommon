#pragma once
#include <cstddef>
#include <cstdint>

namespace ArduinoCommon {
namespace Transport {
enum class IoStatus { PROGRESS, WOULD_BLOCK, CLOSED, ERROR };
struct IoResult { IoStatus status; std::size_t size; };
class ByteStream {
 public:
  virtual ~ByteStream() = default;
  virtual IoResult read(uint8_t* output, std::size_t capacity) = 0;
  virtual IoResult write(const uint8_t* data, std::size_t size) = 0;
  virtual void close() = 0;
};
class Clock {
 public:
  virtual ~Clock() = default;
  virtual uint32_t nowMs() const = 0;
};
}
}
