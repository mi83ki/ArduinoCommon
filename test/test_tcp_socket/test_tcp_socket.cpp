#include <unity.h>
#include <SocketTestApi.h>
#include <transport/TcpSocketESP32.h>
#include <cerrno>
#include <cstring>
using namespace ArduinoCommon::Transport;
namespace {
struct Calls {
  int closes{0}, sockets{0}, reads{0}, writes{0}, selects{0};
  int connectResult{-1}, connectError{EINPROGRESS}, selectResult{1};
  int socketError{0}, getError{0}, flagError{0};
  int readResult{-1}, writeResult{-1}, ioError{EWOULDBLOCK};
  int socketResult{7}, flags{0};
  int noDelayCalls{0}, noDelayError{0};
  bool wifi{true};
  long timeoutUs{0};
  sockaddr_in address{};
} calls;
}
FakeWiFi WiFi;
/** @brief 小分け送信の即時送信オプションを検査し、設定失敗も再現する。 */
int lwip_setsockopt(int, int level, int option, const void* value, socklen_t size) {
  TEST_ASSERT_EQUAL(IPPROTO_TCP, level);
  TEST_ASSERT_EQUAL(TCP_NODELAY, option);
  TEST_ASSERT_EQUAL(sizeof(int), size);
  TEST_ASSERT_EQUAL(1, *static_cast<const int*>(value));
  ++calls.noDelayCalls;
  return calls.noDelayError;
}
/** @brief 偽Wi-Fi状態を返す。 @return 接続状態。 */
int FakeWiFi::status() const { return calls.wifi ? WL_CONNECTED : 0; }
/** @brief 偽時計。 @return 固定ms。 */
std::uint32_t millis() { return 123; }
/** @brief socket生成を記録する。 @return 偽fd。 */
int lwip_socket(int, int, int) { ++calls.sockets; return calls.socketResult; }
/** @brief 非待機フラグを記録する。 @param cmd 操作。 @param flags 指定フラグ。 @return 結果。 */
int lwip_fcntl(int, int cmd, int flags) {
  if ((calls.flagError == 1 && cmd == F_GETFL) ||
      (calls.flagError == 2 && cmd == F_SETFL)) { return -1; }
  if (cmd == F_GETFL) { return 0; }
  calls.flags = flags; return 0;
}
/** @brief 接続先を記録する。 @param addr 接続先。 @return 偽接続結果。 */
int lwip_connect(int, const sockaddr* addr, socklen_t) {
  std::memcpy(&calls.address, addr, sizeof(calls.address));
  errno = calls.connectError; return calls.connectResult;
}
/** @brief 有限待ち時間を記録する。 @param timeout 待ち時間。 @return 偽select結果。 */
int lwip_select(int, fd_set*, fd_set*, fd_set*, timeval* timeout) {
  ++calls.selects;
  calls.timeoutUs = timeout->tv_sec * 1000000L + timeout->tv_usec;
  return calls.selectResult;
}
/** @brief 接続後のSO_ERRORを返す。 @param out 出力先。 @return 取得成否。 */
int lwip_getsockopt(int, int, int, void* out, socklen_t*) {
  *static_cast<int*>(out) = calls.socketError; return calls.getError;
}
/** @brief closeを記録する。 @return 成功。 */
int lwip_close(int) { ++calls.closes; return 0; }
/** @brief 単一受信を記録する。 @return 実byte数またはエラー。 */
int lwip_recv(int, void*, std::size_t, int) {
  ++calls.reads; errno = calls.ioError; return calls.readResult;
}
/** @brief 単一送信を記録する。 @return 実byte数またはエラー。 */
int lwip_send(int, const void*, std::size_t, int) {
  ++calls.writes; errno = calls.ioError; return calls.writeResult;
}
/** @brief 各試験の偽syscallを初期化する。 */
void setUp() { calls = {}; }
/** @brief 後処理。 */
void tearDown() {}
/** @brief 数値IPv4と非待機フラグ、最大1秒のconnect待ちを固定する。 */
void testNonblockingConnectAndLifetime() {
  {
    TcpSocketESP32 socket;
    TEST_ASSERT_FALSE(socket.connect({127, 0, 0, 1}, 23, 1001));
    TEST_ASSERT_FALSE(socket.connect({127, 0, 0, 1}, 0, 1000));
    TEST_ASSERT_EQUAL(0, calls.sockets);
    TEST_ASSERT_TRUE(socket.connect({192, 168, 1, 2}, 1234, 500));
    TEST_ASSERT_EQUAL(O_NONBLOCK, calls.flags & O_NONBLOCK);
    TEST_ASSERT_EQUAL(500000, calls.timeoutUs);
    const uint8_t expected[] = {192, 168, 1, 2};
    TEST_ASSERT_EQUAL_MEMORY(expected, &calls.address.sin_addr.s_addr, 4);
    const uint8_t port[] = {4, 210};
    TEST_ASSERT_EQUAL_MEMORY(port, &calls.address.sin_port, 2);
  }
  TEST_ASSERT_EQUAL(1, calls.closes);
}
/** @brief 接続失敗の全経路でfdを閉じ、Wi-Fi未接続ではsocketを生成しない。 */
void testConnectFailuresCloseDescriptor() {
  for (int failure = 0; failure < 7; ++failure) {
    calls = {};
    TcpSocketESP32 socket;
    if (failure == 0) { calls.flagError = 1; }
    if (failure == 1) { calls.selectResult = 0; }
    if (failure == 2) { calls.socketError = ECONNREFUSED; }
    if (failure == 3) { calls.getError = -1; }
    if (failure == 4) { calls.selectResult = -1; }
    if (failure == 5) { calls.connectError = ECONNREFUSED; }
    if (failure == 6) { calls.flagError = 2; }
    TEST_ASSERT_FALSE(socket.connect({127, 0, 0, 1}, 23));
    TEST_ASSERT_EQUAL(1, calls.closes);
    socket.close();
    TEST_ASSERT_EQUAL(1, calls.closes);
  }
  calls = {};
  calls.wifi = false;
  TcpSocketESP32 socket;
  TEST_ASSERT_FALSE(socket.connect({127, 0, 0, 1}, 23));
  TEST_ASSERT_EQUAL(0, calls.sockets);
  calls.wifi = true;
  calls.socketResult = -1;
  TEST_ASSERT_FALSE(socket.connect({127, 0, 0, 1}, 23));
  TEST_ASSERT_EQUAL(0, calls.closes);
}
/** @brief 部分送受信とwould-blockを1 syscallで返し、EOFと障害を区別する。 */
void testSingleCallIoResults() {
  TcpSocketESP32 socket;
  uint8_t buffer[8]{};
  TEST_ASSERT_EQUAL(int(IoStatus::CLOSED), int(socket.read(buffer, sizeof(buffer)).status));
  calls.connectResult = 0;
  TEST_ASSERT_TRUE(socket.connect({127, 0, 0, 1}, 23));
  TEST_ASSERT_EQUAL(0, calls.selects);
  TEST_ASSERT_EQUAL(int(IoStatus::WOULD_BLOCK), int(socket.read(buffer, sizeof(buffer)).status));
  TEST_ASSERT_EQUAL(int(IoStatus::WOULD_BLOCK), int(socket.write(buffer, sizeof(buffer)).status));
  TEST_ASSERT_EQUAL(1, calls.reads);
  TEST_ASSERT_EQUAL(1, calls.writes);
  calls.readResult = 3;
  calls.writeResult = 2;
  TEST_ASSERT_EQUAL_UINT(3, socket.read(buffer, sizeof(buffer)).size);
  TEST_ASSERT_EQUAL_UINT(2, socket.write(buffer, sizeof(buffer)).size);
  calls.readResult = 0;
  TEST_ASSERT_EQUAL(int(IoStatus::CLOSED), int(socket.read(buffer, sizeof(buffer)).status));
  calls.writeResult = -1;
  calls.ioError = ECONNRESET;
  TEST_ASSERT_EQUAL(int(IoStatus::ERROR), int(socket.write(buffer, sizeof(buffer)).status));
  calls.ioError = EINTR;
  TEST_ASSERT_EQUAL(int(IoStatus::WOULD_BLOCK), int(socket.write(buffer, sizeof(buffer)).status));
  const auto writes = calls.writes;
  const auto reads = calls.reads;
  TEST_ASSERT_EQUAL(int(IoStatus::ERROR), int(socket.write(nullptr, 8).status));
  TEST_ASSERT_EQUAL(int(IoStatus::ERROR), int(socket.read(buffer, 0).status));
  TEST_ASSERT_EQUAL(writes, calls.writes);
  TEST_ASSERT_EQUAL(reads, calls.reads);
  ArduinoMillisClock clock;
  TEST_ASSERT_EQUAL(123, clock.nowMs());
}
/** @brief 即時・非同期接続と再接続で即時送信を設定し、設定失敗では閉じる。 */
void testNoDelayOnEveryConnectionAndFailure() {
  TcpSocketESP32 socket;
  TEST_ASSERT_TRUE(socket.connect({127, 0, 0, 1}, 23));
  TEST_ASSERT_EQUAL(1, calls.noDelayCalls);
  calls.connectResult = 0;
  TEST_ASSERT_TRUE(socket.connect({127, 0, 0, 1}, 23));
  TEST_ASSERT_EQUAL(2, calls.noDelayCalls);
  calls.noDelayError = -1;
  TEST_ASSERT_FALSE(socket.connect({127, 0, 0, 1}, 23));
  TEST_ASSERT_EQUAL(3, calls.noDelayCalls);
  TEST_ASSERT_EQUAL(3, calls.closes);
}
/** @brief ソケット公開APIを実行する。 @return Unity終了コード。 */
int main() {
  UNITY_BEGIN();
  RUN_TEST(testNonblockingConnectAndLifetime);
  RUN_TEST(testConnectFailuresCloseDescriptor);
  RUN_TEST(testSingleCallIoResults);
  RUN_TEST(testNoDelayOnEveryConnectionAndFailure);
  return UNITY_END();
}
