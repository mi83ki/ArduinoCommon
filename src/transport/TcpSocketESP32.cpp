#include "TcpSocketESP32.h"
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINOCOMMON_TEST_ESP32)
#if defined(ARDUINOCOMMON_TEST_ESP32)
#include <SocketTestApi.h>
#else
#include <Arduino.h>
#include <WiFi.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#endif
#include <cerrno>
#include <cstring>

namespace ArduinoCommon {
namespace Transport {
namespace {
/** @brief 1回のsyscall結果を変換する。 @param result 実byte数。 @return 非待機I/O結果。 */
IoResult ioResult(int result) {
  if (result > 0) { return {IoStatus::PROGRESS, static_cast<std::size_t>(result)}; }
  if (result == 0) { return {IoStatus::CLOSED, 0}; }
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
    return {IoStatus::WOULD_BLOCK, 0};
  }
  return {IoStatus::ERROR, 0};
}
}
/** @brief 所有fdを閉じる。 */
TcpSocketESP32::~TcpSocketESP32() { close(); }
/**
 * @brief 数値IPv4へ有限時間で接続する。DNS・Wi-Fi接続は行わない。
 * @param address ネットワーク順のIPv4の4 octet。
 * @param port 接続先port（1〜65535）。
 * @param timeoutMs selectの待ち時間（1〜1000ms）。SDK scheduling遅延は含まない。
 * @return 接続が成立すればtrue。失敗時は旧接続も含めfdを残さない。
 */
bool TcpSocketESP32::connect(const std::array<uint8_t, 4>& address, uint16_t port,
                             uint32_t timeoutMs) {
  close();
  if (port == 0 || timeoutMs == 0 || timeoutMs > 1000 || WiFi.status() != WL_CONNECTED) {
    return false;
  }
  _socket = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (_socket < 0) { return false; }
  // 128 byteずつの制御応答をNagleと相手の遅延ACKで待たせない。
  const int noDelay = 1;
  if (lwip_setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY,
                      &noDelay, sizeof(noDelay)) < 0) {
    close(); return false;
  }
  const int flags = lwip_fcntl(_socket, F_GETFL, 0);
  if (flags < 0 || lwip_fcntl(_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
    close(); return false;
  }
  sockaddr_in target{};
  target.sin_family = AF_INET;
  const uint8_t portBytes[] = {static_cast<uint8_t>(port >> 8), static_cast<uint8_t>(port)};
  std::memcpy(&target.sin_addr.s_addr, address.data(), address.size());
  std::memcpy(&target.sin_port, portBytes, sizeof(portBytes));
  const int result = lwip_connect(_socket, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
  if (result == 0) { return true; }
  if (errno != EINPROGRESS) { close(); return false; }
  fd_set writable;
  FD_ZERO(&writable);
  FD_SET(_socket, &writable);
  timeval timeout{};
  timeout.tv_sec = timeoutMs / 1000;
  timeout.tv_usec = (timeoutMs % 1000) * 1000;
  if (lwip_select(_socket + 1, nullptr, &writable, nullptr, &timeout) <= 0) {
    close(); return false;
  }
  int error = 0;
  socklen_t length = sizeof(error);
  if (lwip_getsockopt(_socket, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) {
    close(); return false;
  }
  return true;
}
/**
 * @brief 非待機recvを最大1回実行する。EAGAIN/EINTRは再試行しない。
 * @param output 出力先。null不可。
 * @param capacity 出力容量。1以上。
 * @return 実受信数、待機必要、EOF、または障害。
 */
IoResult TcpSocketESP32::read(uint8_t* output, std::size_t capacity) {
  if (_socket < 0) { return {IoStatus::CLOSED, 0}; }
  if (output == nullptr || capacity == 0) { return {IoStatus::ERROR, 0}; }
  return ioResult(lwip_recv(_socket, output, capacity, 0));
}
/**
 * @brief 非待機sendを最大1回実行する。部分送信の続きを内部で待たない。
 * @param data 入力先頭。null不可。
 * @param size 入力byte数。1以上。
 * @return 実送信数、待機必要、切断、または障害。
 */
IoResult TcpSocketESP32::write(const uint8_t* data, std::size_t size) {
  if (_socket < 0) { return {IoStatus::CLOSED, 0}; }
  if (data == nullptr || size == 0) { return {IoStatus::ERROR, 0}; }
  return ioResult(lwip_send(_socket, data, size, 0));
}
/** @brief fdを一度だけ閉じる。複数回呼んでも安全。 */
void TcpSocketESP32::close() {
  if (_socket >= 0) { lwip_close(_socket); _socket = -1; }
}
/** @brief Arduinoの単調時計を取得する。 @return 折返しを許容するuint32 ms。 */
uint32_t ArduinoMillisClock::nowMs() const { return static_cast<uint32_t>(millis()); }
}
}
#endif
