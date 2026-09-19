// 製品非依存のLFフレーミング。I/Oは非待機ByteStreamへ限定する。
#include "FramedChannel.h"
#include <algorithm>
#include <cstring>

namespace ArduinoCommon {
namespace Transport {
namespace {
constexpr std::size_t BYTE_BUDGET = 512;
constexpr unsigned CALL_BUDGET = 8;
constexpr uint32_t TIME_BUDGET_MS = 4;
constexpr uint32_t SEND_TIMEOUT_MS = 1000;
constexpr uint32_t RECEIVE_TIMEOUT_MS = 5000;
}
/**
 * @brief 接続済みstreamと単調時計を借用する。resetまでは停止状態。
 * @param stream 全APIが待機しないstream。channelより長く生存すること。
 * @param clock uint32 ms時計。channelより長く生存すること。
 */
FramedChannel::FramedChannel(ByteStream& stream, Clock& clock)
    : stream_(stream), clock_(clock) {}

/** @brief 新接続開始時に全途中状態を破棄する。接続操作自体は行わない。 */
void FramedChannel::reset() {
  receiveSize_ = sendSize_ = sentSize_ = 0;
  pendingSize_ = pendingPosition_ = 0;
  receive_[0] = '\0';
  ready_ = false;
  error_ = ChannelError::NONE;
}
/** @brief streamを閉じ、完成/途中/送信待ちの全状態を無効化する。 */
void FramedChannel::disconnect() { fail(ChannelError::DISCONNECTED); }
/** @brief 接続を一度だけ閉じる。 @param reason 保持する障害理由。 */
void FramedChannel::fail(ChannelError reason) {
  if (error_ != ChannelError::NONE) { return; }
  stream_.close();
  reset();
  error_ = reason;
}
/** @brief 絶対経過時間の期限を検査する。 @return 稼働可能ならtrue。 */
bool FramedChannel::checkTimeouts() {
  const auto now = clock_.nowMs();
  if (sendPending() && uint32_t(now - sendStarted_) >= SEND_TIMEOUT_MS) {
    fail(ChannelError::SEND_TIMEOUT);
  } else if (!ready_ && receiveSize_ &&
             uint32_t(now - receiveStarted_) >= RECEIVE_TIMEOUT_MS) {
    fail(ChannelError::RECEIVE_TIMEOUT);
  }
  return error_ == ChannelError::NONE;
}
/**
 * @brief driverの戻り値を検査し、切断/矛盾は接続障害にする。
 * @param result driverの結果。
 * @param capacity 渡した最大byte数。
 * @return PROGRESSまたはWOULD_BLOCKが整合していればtrue。
 */
bool FramedChannel::validResult(const IoResult& result, std::size_t capacity) {
  if (result.status == IoStatus::CLOSED) {
    fail(ChannelError::DISCONNECTED);
  } else if (result.status == IoStatus::ERROR) {
    fail(ChannelError::IO_ERROR);
  } else if ((result.status != IoStatus::PROGRESS && result.status != IoStatus::WOULD_BLOCK) ||
             result.size > capacity ||
             (result.status == IoStatus::PROGRESS && result.size == 0) ||
             (result.status == IoStatus::WOULD_BLOCK && result.size != 0)) {
    fail(ChannelError::INVALID_DRIVER_RESULT);
  }
  return error_ == ChannelError::NONE;
}
/**
 * @brief 1件のframeを所有バッファへコピーする。送信成功を意味しない。
 * @param data 入力先頭。null不可。LFは含めない。
 * @param size LFを除く0〜4096 byte。
 * @return 受付結果。BUSY等の場合は既存送信を変更しない。
 */
QueueResult FramedChannel::send(const char* data, std::size_t size) {
  if (error_ != ChannelError::NONE) { return QueueResult::CLOSED; }
  if (data == nullptr || size > MAX_FRAME_SIZE || std::memchr(data, '\n', size)) {
    return QueueResult::INVALID;
  }
  if (sendPending()) { return QueueResult::BUSY; }
  std::memcpy(send_.data(), data, size);
  send_[size] = '\n';
  sendSize_ = size + 1;
  sentSize_ = 0;
  sendStarted_ = clock_.nowMs();
  return QueueResult::QUEUED;
}
/**
 * @brief 有限I/O予算内で送受信する。完成受信はconsumeFrameまで保持する。
 * @note 最大512 I/O byte、8 syscall、4ms。各非待機callの前後で期限を確認する。
 *       送信は1回最大128 byteとして受信の進行を妨げない。
 */
void FramedChannel::poll() {
  if (error_ != ChannelError::NONE || !checkTimeouts()) { return; }
  const uint32_t started = clock_.nowMs();
  std::size_t bytes = 0;
  unsigned calls = 0;
  if (sendPending()) {
    const auto limit = std::min<std::size_t>(128, sendSize_ - sentSize_);
    const auto result = stream_.write(send_.data() + sentSize_, limit);
    ++calls;
    if (!validResult(result, limit) || !checkTimeouts()) { return; }
    sentSize_ += result.size;
    bytes += result.size;
  }
  while (!ready_ && uint32_t(clock_.nowMs() - started) < TIME_BUDGET_MS) {
    if (!checkTimeouts()) { return; }
    if (pendingPosition_ == pendingSize_) {
      if (bytes >= BYTE_BUDGET || calls >= CALL_BUDGET) { return; }
      const auto limit = std::min(pending_.size(), BYTE_BUDGET - bytes);
      const auto result = stream_.read(pending_.data(), limit);
      ++calls;
      if (!validResult(result, limit) || !checkTimeouts()) { return; }
      if (result.status == IoStatus::WOULD_BLOCK) { return; }
      pendingPosition_ = 0;
      pendingSize_ = result.size;
      bytes += result.size;
    }
    // 新たなsyscallなしで先読みを消費する。LF以後のbyteは次frameまで保持。
    while (pendingPosition_ < pendingSize_ && !ready_) {
      const char value = static_cast<char>(pending_[pendingPosition_++]);
      if (value == '\n') {
        receive_[receiveSize_] = '\0';
        ready_ = true;
      } else if (receiveSize_ == MAX_FRAME_SIZE) {
        fail(ChannelError::FRAME_TOO_LONG);
        return;
      } else {
        if (receiveSize_ == 0) { receiveStarted_ = clock_.nowMs(); }
        receive_[receiveSize_++] = value;
      }
    }
  }
}
/** @brief 未送信byteが残るか返す。 @return 残っていればtrue。 */
bool FramedChannel::sendPending() const { return sentSize_ < sendSize_; }
/** @brief 完成した受信の有無。 @return consume前のframeがあればtrue。 */
bool FramedChannel::hasFrame() const { return ready_; }
/** @brief 完成した受信を借用する。 @return consume/reset/disconnectまで有効な先頭。 */
const char* FramedChannel::frameData() const { return receive_.data(); }
/** @brief 完成frameの長さ。 @return LFを除くbyte数。未完成時0。 */
std::size_t FramedChannel::frameSize() const { return ready_ ? receiveSize_ : 0; }
/** @brief 完成frameを解放する。未完成の部分受信には影響しない。 */
void FramedChannel::consumeFrame() {
  if (ready_) {
    ready_ = false;
    receiveSize_ = 0;
    receive_[0] = '\0';
  }
}
/** @brief 最初の障害を取得する。 @return resetまで保持する障害理由。 */
ChannelError FramedChannel::error() const { return error_; }
}
}
