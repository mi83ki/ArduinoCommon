#include "TCPClientESP32.h"

#include "Log.h"

/**
 * @brief TCPClientESP32 オブジェクトを生成する。
 *
 * @param serverIP 接続先サーバーのIPアドレスまたはホスト名
 * @param serverPort 接続先サーバーのポート番号
 * @param endCharacter 送受信メッセージの終端文字
 * @param reconnectIntervalMs 再接続を試行する最短間隔[ms]
 */
TCPClientESP32::TCPClientESP32(const char *serverIP, uint16_t serverPort,
                               char endCharacter,
                               uint32_t reconnectIntervalMs)
    : _tcp(new WiFiClient()),
      _tcpFlag(0),
      _serverIP(serverIP),
      _serverPort(serverPort),
      _endCharacter(endCharacter),
      _reconnectIntervalMs(reconnectIntervalMs),
      _timer(Timer()) {}

/**
 * @brief TCPClientESP32 オブジェクトを破棄する。
 */
TCPClientESP32::~TCPClientESP32() {
  _tcp->stop();
  delete _tcp;
}

/**
 * @brief TCP接続を開始する。
 *
 * @return true 開始処理成功
 */
bool TCPClientESP32::begin(void) {
  connectedAction();
  return true;
}

/**
 * @brief 未接続の場合にTCP接続を試行する。
 */
void TCPClientESP32::connectedAction(void) {
  if (_tcpFlag == 0 && (_timer.getTime() >= _reconnectIntervalMs)) {
    _tcpFlag = _tcp->connect(_serverIP, _serverPort);
    if (_tcpFlag) {
      String msg = "TCP:Connected:";
      msg.concat(_serverIP);
      logger.info(msg);
    } else {
      logger.info("TCP:Couldn't connect.");
      _tcpFlag = 0;
      _tcp->stop();
      _timer.startTimer();
    }
  }
}

/**
 * @brief TCP接続を切断する。
 */
void TCPClientESP32::disconnectedAction(void) {
  if (_tcpFlag == 1) {
    _tcpFlag = 0;
    _tcp->stop();
    logger.info("TCP:Disconnected.");
    _timer.startTimer();
  }
}

/**
 * @brief TCPで文字列を送信する。
 *
 * @param str 送信文字列
 * @return true 送信成功
 * @return false 送信失敗
 */
bool TCPClientESP32::sendString(String str) {
  if (!_tcpFlag) {
    _tcpFlag = _tcp->connect(_serverIP, _serverPort);
    if (!_tcpFlag) {
      logger.error("TCPClientESP32::sendString(): reconnect failed.");
      return false;
    }
  }

  logger.info("TCP Posting: " + str);
  _tcp->print(str + _endCharacter);
  return true;
}

/**
 * @brief 受信可能なバイト数を取得する。
 *
 * @return uint16_t 受信可能なバイト数
 */
uint16_t TCPClientESP32::available(void) { return _tcp->available(); }

/**
 * @brief TCPメッセージを受信しているかどうかを取得する。
 *
 * @return uint16_t 受信可能なバイト数
 */
uint16_t TCPClientESP32::isReceived(void) { return available(); }

/**
 * @brief TCPメッセージを受信しているかどうかを取得する。
 *
 * @return uint16_t 受信可能なバイト数
 */
uint16_t TCPClientESP32::isRecieved(void) { return isReceived(); }

/**
 * @brief 終端文字までのTCP受信文字列を取得する。
 *
 * @return String 受信文字列
 */
String TCPClientESP32::readString(void) {
  if (isReceived()) {
    String buf = _tcp->readStringUntil(_endCharacter);
    logger.info("recv: " + buf);
    return buf;
  }

  logger.info("TCPClientESP32::readString(): no message");
  return "";
}
