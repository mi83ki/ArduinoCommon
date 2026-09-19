#pragma once
#include <array>
#include "ByteStream.h"

namespace ArduinoCommon {
namespace Transport {
enum class ChannelError {
  NONE, DISCONNECTED, FRAME_TOO_LONG, IO_ERROR,
  SEND_TIMEOUT, RECEIVE_TIMEOUT, INVALID_DRIVER_RESULT
};
enum class QueueResult { QUEUED, BUSY, INVALID, CLOSED };
class FramedChannel {
 public:
  static constexpr std::size_t MAX_FRAME_SIZE = 4096;
  FramedChannel(ByteStream& stream, Clock& clock);
  FramedChannel(const FramedChannel&) = delete;
  FramedChannel& operator=(const FramedChannel&) = delete;
  void reset();
  void disconnect();
  void poll();
  QueueResult send(const char* data, std::size_t size);
  bool sendPending() const;
  bool hasFrame() const;
  const char* frameData() const;
  std::size_t frameSize() const;
  void consumeFrame();
  ChannelError error() const;

 private:
  void fail(ChannelError reason);
  bool checkTimeouts();
  bool validResult(const IoResult& result, std::size_t capacity);
  ByteStream& stream_;
  Clock& clock_;
  std::array<char, MAX_FRAME_SIZE + 1> receive_{};
  std::array<uint8_t, MAX_FRAME_SIZE + 1> send_{};
  std::array<uint8_t, 128> pending_{};
  std::size_t receiveSize_{0}, sendSize_{0}, sentSize_{0};
  std::size_t pendingSize_{0}, pendingPosition_{0};
  uint32_t receiveStarted_{0}, sendStarted_{0};
  bool ready_{false};
  ChannelError error_{ChannelError::DISCONNECTED};
};
}
}
