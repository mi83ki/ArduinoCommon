// 旧TCPの公開動作を変更前の本番cppで固定する。
#include <unity.h>
#include "TCPClientESP32.h"
#include "Log.h"

uint32_t fakeMillis = 0;
TcpFake tcpFake;
Log logger;
/** @brief 試験用ログの初期化。 */
Log::Log() {}
/** @brief 試験用ログの終了。 */
Log::~Log() {}
/** @brief ログを抑制する。 @param msg 未使用のログ。 */
void Log::info(String msg) { (void)msg; }
/** @brief ログを抑制する。 @param msg 未使用のログ。 */
void Log::error(String msg) { (void)msg; }
/** @brief 時計とソケット記録を初期化する。 */
void setUp() { fakeMillis = 0; tcpFake = TcpFake{}; }
/** @brief 試験後処理。 */
void tearDown() {}

/** @brief 旧sendは終端を付け、printの部分送信/0戻り値を成功扱いする。 */
void testLegacySendContract() {
  TCPClientESP32 client("192.0.2.1", 1234);
  TEST_ASSERT_TRUE(client.sendString("abc"));
  TEST_ASSERT_EQUAL_STRING("abc\n", tcpFake.output.c_str());
  TEST_ASSERT_EQUAL_UINT(1, tcpFake.connects);
  tcpFake.writeCount = 2;
  TEST_ASSERT_TRUE(client.sendString("abc"));
  TEST_ASSERT_EQUAL_STRING("abc\nabc\n", tcpFake.output.c_str());
  client.disconnectedAction();
  tcpFake.connectResult = false;
  TEST_ASSERT_FALSE(client.sendString("def"));
}
/** @brief 旧readはdelimiterまで読み、availableの旧綴りaliasも維持する。 */
void testLegacyReadContract() {
  TCPClientESP32 client("192.0.2.1", 1234, ';');
  TEST_ASSERT_EQUAL_STRING("", client.readString().c_str());
  TEST_ASSERT_EQUAL_UINT(0, tcpFake.reads);
  tcpFake.input = "abc;def;";
  TEST_ASSERT_EQUAL_UINT(8, client.available());
  TEST_ASSERT_EQUAL_UINT(8, client.isReceived());
  TEST_ASSERT_EQUAL_UINT(8, client.isRecieved());
  TEST_ASSERT_EQUAL_STRING("abc", client.readString().c_str());
  TEST_ASSERT_EQUAL_CHAR(';', tcpFake.delimiter);
  TEST_ASSERT_EQUAL_STRING("def", client.readString().c_str());
}
/** @brief 接続失敗でもbeginはtrueで、指定間隔後にのみ再試行する旧動作。 */
void testLegacyReconnectContract() {
  TCPClientESP32 client("192.0.2.1", 1234, '\n', 50);
  tcpFake.connectResult = false;
  TEST_ASSERT_TRUE(client.begin());
  TEST_ASSERT_EQUAL_UINT(0, tcpFake.connects);
  fakeMillis = 50;
  client.connectedAction();
  TEST_ASSERT_EQUAL_UINT(1, tcpFake.connects);
  fakeMillis = 99;
  client.connectedAction();
  TEST_ASSERT_EQUAL_UINT(1, tcpFake.connects);
  fakeMillis = 100;
  client.connectedAction();
  TEST_ASSERT_EQUAL_UINT(2, tcpFake.connects);
}
/** @brief 旧TCP特性を実行する。 @return Unity終了コード。 */
int main() {
  UNITY_BEGIN();
  RUN_TEST(testLegacySendContract);
  RUN_TEST(testLegacyReadContract);
  RUN_TEST(testLegacyReconnectContract);
  return UNITY_END();
}
