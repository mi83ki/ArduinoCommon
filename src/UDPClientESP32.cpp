#include "UDPClientESP32.h"

#include "Log.h"

/**
 * @brief UDPClientESP32 オブジェクトを生成する。
 *
 * @param serverIP 送信先サーバーのIPアドレスまたはホスト名
 * @param serverPort 送信先サーバーのポート番号
 * @param clientPort 自デバイス側のUDPポート番号
 */
UDPClientESP32::UDPClientESP32(const char *serverIP, uint16_t serverPort,
                               uint16_t clientPort)
    : _udp(new WiFiUDP()),
      _udpFlag(0),
      _serverIP(serverIP),
      _serverPort(serverPort),
      _clientPort(clientPort) {}

/**
 * @brief UDPClientESP32 オブジェクトを破棄する。
 */
UDPClientESP32::~UDPClientESP32() {
  _udp->stop();
  delete _udp;
}

/**
 * @brief UDP通信を開始する。
 *
 * @return true 開始処理成功
 */
bool UDPClientESP32::begin(void) {
  connectedAction();
  return true;
}

/**
 * @brief UDP待ち受けを開始する。
 */
void UDPClientESP32::connectedAction(void) {
  if (_udpFlag == 0) {
    _udpFlag = 1;
    _udp->begin(_clientPort);
    logger.info("UDP:Connected.");
  }
}

/**
 * @brief UDP待ち受けを停止する。
 */
void UDPClientESP32::disconnectedAction(void) {
  if (_udpFlag == 1) {
    _udpFlag = 0;
    _udp->stop();
    logger.info("UDP:Disconnected.");
  }
}

/**
 * @brief UDPでバイト列を送信する。
 *
 * @param data 送信データ
 * @param size 送信サイズ[byte]
 * @return true 送信成功
 * @return false 送信失敗
 */
bool UDPClientESP32::send(const uint8_t *data, size_t size) {
  if (_udp->beginPacket(_serverIP, _serverPort) != 0) {
    _udp->write(data, size);
    _udp->endPacket();
    return true;
  }

  logger.error("UDPClientESP32::send(): beginPacket failed.");
  return false;
}

/**
 * @brief UDPでバイト列を送信する。
 *
 * @param data 送信データ
 * @param size 送信サイズ[byte]
 * @return true 送信成功
 * @return false 送信失敗
 */
bool UDPClientESP32::sendUDP(uint8_t *data, uint8_t size) {
  return send(data, size);
}
