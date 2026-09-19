// 偽の非待機streamと時計を使い、本番フレーム処理だけを検証する。
#include <unity.h>
#include <algorithm>
#include <cstring>
#include <string>
#include "transport/FramedChannel.h"
using namespace ArduinoCommon::Transport;

struct FakeClock : Clock {
  uint32_t now{0};
  /** @brief 単調時計を返す。 @return ms。 */
  uint32_t nowMs() const override { return now; }
};
struct FakeStream : ByteStream {
  FakeClock* clock{nullptr};
  std::string input, output;
  size_t readLimit{4096}, writeLimit{4096};
  unsigned reads{0}, writes{0}, closes{0};
  bool eof{false}, broken{false};
  bool invalid{false}, writeClosed{false};
  uint32_t cost{0};
  /** @brief 偽受信を行う。 @param out 出力先。 @param size 上限。 @return 状態とbyte数。 */
  IoResult read(uint8_t* out, size_t size) override {
    ++reads;
    if (clock) { clock->now += cost; }
    if (broken) { return {IoStatus::ERROR, 0}; }
    if (invalid) { return {IoStatus::PROGRESS, size + 1}; }
    const auto count = std::min({size, input.size(), readLimit});
    if (count) {
      std::memcpy(out, input.data(), count);
      input.erase(0, count);
      return {IoStatus::PROGRESS, count};
    }
    return {eof ? IoStatus::CLOSED : IoStatus::WOULD_BLOCK, 0};
  }
  /** @brief 偽部分送信を行う。 @param data 入力先頭。 @param size 長さ。 @return 実送信数。 */
  IoResult write(const uint8_t* data, size_t size) override {
    ++writes;
    if (clock) { clock->now += cost; }
    if (broken) { return {IoStatus::ERROR, 0}; }
    if (writeClosed) { return {IoStatus::CLOSED, 0}; }
    const auto count = std::min(size, writeLimit);
    output.append(reinterpret_cast<const char*>(data), count);
    return {count ? IoStatus::PROGRESS : IoStatus::WOULD_BLOCK, count};
  }
  /** @brief 切断回数を記録する。 */
  void close() override { ++closes; }
};
/** @brief 前処理（試験ごとのローカル状態）。 */
void setUp() {}
/** @brief 後処理。 */
void tearDown() {}

/** @brief 分割・連結と先読みを、完成frameの所有期間を保って処理する。 */
void testSplitAndConcatenatedFrames() {
  FakeClock clock;
  FakeStream stream;
  FramedChannel channel(stream, clock);
  channel.reset();
  stream.input = "ab";
  channel.poll();
  TEST_ASSERT_FALSE(channel.hasFrame());
  stream.input = "c\ndef\n\n";
  channel.poll();
  TEST_ASSERT_TRUE(channel.hasFrame());
  TEST_ASSERT_EQUAL_STRING("abc", channel.frameData());
  channel.poll();
  TEST_ASSERT_EQUAL_STRING("abc", channel.frameData());
  channel.consumeFrame();
  channel.poll();
  TEST_ASSERT_EQUAL_STRING("def", channel.frameData());
  channel.consumeFrame();
  channel.poll();
  TEST_ASSERT_TRUE(channel.hasFrame());
  TEST_ASSERT_EQUAL_UINT(0, channel.frameSize());
}
/** @brief LFを除く4096 byteを受理し、4097 byteでは切断して全途中状態を捨てる。 */
void testFrameLimit() {
  FakeClock clock;
  FakeStream stream;
  FramedChannel channel(stream, clock);
  channel.reset();
  stream.input.assign(4096, 'a');
  stream.input += '\n';
  for (unsigned i = 0; i < 16; ++i) { channel.poll(); }
  TEST_ASSERT_TRUE(channel.hasFrame());
  TEST_ASSERT_EQUAL_UINT(4096, channel.frameSize());
  channel.consumeFrame();
  stream.input.assign(4097, 'a');
  for (unsigned i = 0; i < 16; ++i) { channel.poll(); }
  TEST_ASSERT_EQUAL(static_cast<int>(ChannelError::FRAME_TOO_LONG), static_cast<int>(channel.error()));
  TEST_ASSERT_EQUAL_UINT(1, stream.closes);
  TEST_ASSERT_FALSE(channel.hasFrame());
}
/** @brief 部分送信でもLFを一度だけ付け、未送信中は次のframeを受け付けない。 */
void testPartialSendAndOwnership() {
  FakeClock clock;
  FakeStream stream;
  stream.writeLimit = 2;
  FramedChannel channel(stream, clock);
  channel.reset();
  char message[] = "abcde";
  TEST_ASSERT_EQUAL(0, static_cast<int>(channel.send(message, 5)));
  message[0] = 'x';
  TEST_ASSERT_EQUAL(static_cast<int>(QueueResult::BUSY), static_cast<int>(channel.send("next", 4)));
  channel.poll();
  TEST_ASSERT_TRUE(channel.sendPending());
  for (unsigned i = 0; i < 4; ++i) { channel.poll(); }
  TEST_ASSERT_FALSE(channel.sendPending());
  TEST_ASSERT_EQUAL_STRING("abcde\n", stream.output.c_str());
  TEST_ASSERT_EQUAL(static_cast<int>(QueueResult::INVALID), static_cast<int>(channel.send("a\nb", 3)));
}
/** @brief would-blockを再試行し続けず、送信は1000msで期限超過にする。 */
void testSendDeadlineAndClockWrap() {
  FakeClock clock;
  clock.now = 0xFFFFFF00U;
  FakeStream stream;
  stream.writeLimit = 0;
  FramedChannel channel(stream, clock);
  channel.reset();
  channel.send("x", 1);
  channel.poll();
  TEST_ASSERT_EQUAL_UINT(1, stream.writes);
  clock.now += 999;
  channel.poll();
  TEST_ASSERT_TRUE(channel.sendPending());
  clock.now += 1;
  channel.poll();
  TEST_ASSERT_EQUAL(static_cast<int>(ChannelError::SEND_TIMEOUT), static_cast<int>(channel.error()));
  TEST_ASSERT_EQUAL_UINT(1, stream.closes);
}
/** @brief 未完了受信の切断・5000ms期限と再接続時の破棄を確認する。 */
void testPartialReceiveDeadlineAndDisconnect() {
  FakeClock clock;
  FakeStream stream;
  FramedChannel channel(stream, clock);
  channel.reset();
  stream.input = "old";
  channel.poll();
  clock.now = 4999;
  channel.poll();
  TEST_ASSERT_EQUAL(0, static_cast<int>(channel.error()));
  clock.now = 5000;
  channel.poll();
  TEST_ASSERT_EQUAL(static_cast<int>(ChannelError::RECEIVE_TIMEOUT), static_cast<int>(channel.error()));
  channel.reset();
  stream.input = "new\npartial";
  stream.eof = true;
  channel.poll();
  TEST_ASSERT_EQUAL_STRING("new", channel.frameData());
  channel.consumeFrame();
  channel.poll();
  TEST_ASSERT_EQUAL(static_cast<int>(ChannelError::DISCONNECTED), static_cast<int>(channel.error()));
  channel.reset();
  stream.eof = false;
  stream.input = "clean\n";
  channel.poll();
  TEST_ASSERT_EQUAL_STRING("clean", channel.frameData());
}
/** @brief byte数と時間予算を超えて1回のpollを続けない。 */
void testPollBudgets() {
  FakeClock clock;
  FakeStream stream;
  stream.input.assign(4000, 'a');
  FramedChannel channel(stream, clock);
  channel.reset();
  channel.poll();
  TEST_ASSERT_EQUAL_UINT(3488, stream.input.size());
  TEST_ASSERT_LESS_OR_EQUAL_UINT(8, stream.reads);
  stream.clock = &clock;
  stream.cost = 4;
  const auto before = stream.reads;
  channel.poll();
  TEST_ASSERT_EQUAL_UINT(before + 1, stream.reads);
}
/** @brief 公開フレームAPIの試験を実行する。 @return Unity終了コード。 */
/** @brief 先読みされた次の部分frameも受信時刻を期限起点にする。 */
void testPrefetchedPartialDeadline() {
  FakeClock clock;
  FakeStream stream;
  FramedChannel channel(stream, clock);
  channel.reset();
  stream.input = "ok\nx";
  channel.poll();
  clock.now = 6000;
  channel.consumeFrame();
  channel.poll();
  TEST_ASSERT_EQUAL(int(ChannelError::RECEIVE_TIMEOUT), int(channel.error()));
}
/** @brief 小さいI/Oの8call制限と送受信合算のbyte/time予算を固定する。 */
void testCombinedBudgets() {
  FakeClock clock;
  FakeStream stream;
  FramedChannel channel(stream, clock);
  channel.reset();
  stream.input.assign(4000, 'a');
  stream.readLimit = 1;
  channel.send("x", 1);
  channel.poll();
  TEST_ASSERT_EQUAL_UINT(8, stream.reads + stream.writes);
  stream.readLimit = 4096;
  const auto remaining = stream.input.size();
  std::string payload(512, 'b');
  channel.send(payload.data(), payload.size());
  channel.poll();
  TEST_ASSERT_EQUAL_UINT(remaining - 384, stream.input.size());
  stream.clock = &clock;
  stream.cost = 4;
  const auto reads = stream.reads;
  channel.poll();
  TEST_ASSERT_EQUAL_UINT(reads, stream.reads);
}
/** @brief 部分進捗や時計折返しでも絶対期限を延長しない。 */
void testProgressDoesNotExtendDeadlines() {
  FakeClock clock;
  clock.now = 0xffffff00U;
  FakeStream stream;
  stream.writeLimit = 1;
  FramedChannel channel(stream, clock);
  channel.reset();
  channel.send("abcde", 5);
  channel.poll();
  clock.now += 999;
  channel.poll();
  clock.now += 1;
  channel.poll();
  TEST_ASSERT_EQUAL(int(ChannelError::SEND_TIMEOUT), int(channel.error()));
  clock.now = 0xffffff00U;
  channel.reset();
  stream.input = "a";
  channel.poll();
  clock.now += 4999;
  stream.input = "b";
  channel.poll();
  clock.now += 1;
  stream.input = "\n";
  channel.poll();
  TEST_ASSERT_EQUAL(int(ChannelError::RECEIVE_TIMEOUT), int(channel.error()));
}
/** @brief 障害の種類によらず送受信と先読みを破棄し一度だけcloseする。 */
void testFailuresDiscardAllState() {
  for (unsigned mode = 0; mode < 4; ++mode) {
    FakeClock clock;
    FakeStream stream;
    FramedChannel channel(stream, clock);
    channel.reset();
    stream.writeLimit = 1;
    channel.send("pending", 7);
    stream.input = "ready\nold";
    channel.poll();
    TEST_ASSERT_TRUE(channel.hasFrame());
    if (mode == 0) { channel.disconnect(); }
    if (mode == 1) { stream.writeClosed = true; channel.poll(); }
    if (mode == 2) { stream.broken = true; channel.poll(); }
    if (mode == 3) { stream.invalid = true; channel.consumeFrame(); channel.poll(); }
    TEST_ASSERT_FALSE(channel.sendPending());
    TEST_ASSERT_FALSE(channel.hasFrame());
    TEST_ASSERT_NOT_EQUAL(int(ChannelError::NONE), int(channel.error()));
    channel.disconnect();
    TEST_ASSERT_EQUAL_UINT(1, stream.closes);
    stream.invalid = stream.broken = stream.writeClosed = false;
    channel.reset();
    stream.input = "new\n";
    channel.poll();
    TEST_ASSERT_EQUAL_STRING("new", channel.frameData());
  }
}
/** @brief 送信境界とwould-block後の再開を確認する。 */
void testSendSizeAndResume() {
  FakeClock clock;
  FakeStream stream;
  FramedChannel channel(stream, clock);
  channel.reset();
  std::string payload(4097, 'z');
  TEST_ASSERT_EQUAL(int(QueueResult::INVALID), int(channel.send(nullptr, 0)));
  TEST_ASSERT_EQUAL(int(QueueResult::INVALID), int(channel.send(payload.data(), 4097)));
  TEST_ASSERT_EQUAL(int(QueueResult::QUEUED), int(channel.send("", 0)));
  channel.poll();
  TEST_ASSERT_EQUAL_STRING("\n", stream.output.c_str());
  stream.output.clear();
  channel.send(payload.data(), 4096);
  channel.poll();
  stream.writeLimit = 0;
  channel.poll();
  stream.writeLimit = 4096;
  for (unsigned i = 0; i < 40; ++i) { channel.poll(); }
  TEST_ASSERT_FALSE(channel.sendPending());
  TEST_ASSERT_EQUAL_UINT(4097, stream.output.size());
  TEST_ASSERT_EQUAL_CHAR('\n', stream.output.back());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(testSplitAndConcatenatedFrames);
  RUN_TEST(testFrameLimit);
  RUN_TEST(testPartialSendAndOwnership);
  RUN_TEST(testSendDeadlineAndClockWrap);
  RUN_TEST(testPartialReceiveDeadlineAndDisconnect);
  RUN_TEST(testPollBudgets);
  RUN_TEST(testPrefetchedPartialDeadline);
  RUN_TEST(testCombinedBudgets);
  RUN_TEST(testProgressDoesNotExtendDeadlines);
  RUN_TEST(testFailuresDiscardAllState);
  RUN_TEST(testSendSizeAndResume);
  return UNITY_END();
}
