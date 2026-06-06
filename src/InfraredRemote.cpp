#include "InfraredRemote.h"

#ifndef UNIT_TEST

#include <Log.h>

#define DECODE_RC6
#define IR_REMOTE_INCLUDE
#include <IRremote.h>

/**
 * @brief 受信ピン、送信ピン、プロトコル、送信ビット長を指定して赤外通信を初期化する。
 *
 * @param receivePin IR 受信ピン
 * @param sendPin IR 送信ピン
 * @param protocol 送信プロトコル
 * @param dataBits 送信ビット長
 */
InfraredRemote::InfraredRemote(uint8_t receivePin, uint8_t sendPin,
                               int8_t protocol, uint8_t dataBits)
    : receivedData_{0, 0},
      txData_(0),
      receivePin_(receivePin),
      sendPin_(sendPin),
      protocol_(protocol),
      dataBits_(dataBits),
      requestSend_(false),
      debug_(false) {
  pinMode(receivePin_, INPUT);
  pinMode(sendPin_, OUTPUT);
  IrReceiver.begin(receivePin_, ENABLE_LED_FEEDBACK);
  IrSender.begin(sendPin_, ENABLE_LED_FEEDBACK);
}

/**
 * @brief 赤外通信オブジェクトを破棄する。
 */
InfraredRemote::~InfraredRemote() = default;

/**
 * @brief 最後に受信した raw data を取得する。
 *
 * @return int32_t 受信 raw data
 */
int32_t InfraredRemote::getData(void) const {
  return static_cast<int32_t>(receivedData_.rawData);
}

/**
 * @brief 最後に受信したプロトコルを取得する。
 *
 * @return int8_t 受信プロトコル
 */
int8_t InfraredRemote::getFormat(void) const { return receivedData_.protocol; }

/**
 * @brief 最後に受信した赤外データ一式を取得する。
 *
 * @return ReceivedData 受信 raw data と protocol
 */
InfraredRemote::ReceivedData InfraredRemote::getReceivedData(void) const {
  return receivedData_;
}

/**
 * @brief 保持している受信データを初期値へ戻す。
 */
void InfraredRemote::clearReceivedData(void) {
  receivedData_.rawData = 0;
  receivedData_.protocol = 0;
}

/**
 * @brief デバッグ出力の有効・無効を切り替える。
 *
 * @param debug true で有効、false で無効
 */
void InfraredRemote::setDebug(bool debug) { debug_ = debug; }

/**
 * @brief IRremote で 1 フレーム受信し、raw data と protocol を保持する。
 *
 * @return true 受信した場合
 * @return false 受信しなかった場合
 */
bool InfraredRemote::recieveIR(void) {
  if (!IrReceiver.decode()) {
    clearReceivedData();
    return false;
  }

  receivedData_.rawData =
      static_cast<uint32_t>(IrReceiver.decodedIRData.decodedRawData);
  receivedData_.protocol = static_cast<int8_t>(IrReceiver.decodedIRData.protocol);
  IrReceiver.resume();

  logger.debug("InfraredRemote::recieveIR(): protocol=" +
               String(receivedData_.protocol) +
               ", raw=" + String(receivedData_.rawData));
  return true;
}

void InfraredRemote::printDebug(void) {
  Serial.println(String(millis()) + ", " + String(receivedData_.protocol) +
                 ", 0b " + String(receivedData_.rawData, BIN));
}

/**
 * @brief 設定されたプロトコルに従って赤外送信する。
 *
 * 現在は XSA が利用している RC6 raw 送信のみをサポートする。
 *
 * @param data 送信 raw data
 * @param nbits 送信ビット長
 */
void InfraredRemote::sendIR(uint32_t data, int nbits) {
  switch (protocol_) {
    case RC6:
      IrSender.sendRC6(data, nbits);
      break;
    default:
      logger.error("InfraredRemote::sendIR(): unsupported protocol=" +
                   String(protocol_));
      return;
  }

  logger.debug("InfraredRemote::sendIR(): protocol=" + String(protocol_) +
               ", raw=" + String(data) + ", bits=" + String(nbits));
}

/**
 * @brief 次回 `drive()` 呼び出し時に送信する raw data を予約する。
 *
 * @param data 送信 raw data
 */
void InfraredRemote::requestSend(uint32_t data) {
  txData_ = data;
  requestSend_ = true;
}

/**
 * @brief 送信要求の有無を取得し、要求済み状態を消費する。
 *
 * @return true 送信要求があった場合
 * @return false 送信要求がない場合
 */
bool InfraredRemote::isRequestSend(void) {
  if (!requestSend_) {
    return false;
  }
  requestSend_ = false;
  return true;
}

/**
 * @brief 送信要求があれば予約済みデータを 1 回送信する。
 *
 * @return true 送信した場合
 * @return false 送信要求がなかった場合
 */
bool InfraredRemote::drive(void) {
  if (!isRequestSend()) {
    return false;
  }
  sendIR(txData_, dataBits_);
  return true;
}

/**
 * @brief 受信ピン番号を取得する。
 *
 * @return uint8_t 受信ピン番号
 */
uint8_t InfraredRemote::getReceivePin(void) const { return receivePin_; }

/**
 * @brief 送信ピン番号を取得する。
 *
 * @return uint8_t 送信ピン番号
 */
uint8_t InfraredRemote::getSendPin(void) const { return sendPin_; }

/**
 * @brief 設定済みの送信プロトコルを取得する。
 *
 * @return int8_t 送信プロトコル
 */
int8_t InfraredRemote::getProtocol(void) const { return protocol_; }

/**
 * @brief 設定済みの送信ビット長を取得する。
 *
 * @return uint8_t 送信ビット長
 */
uint8_t InfraredRemote::getDataBits(void) const { return dataBits_; }

/**
 * @brief 予約済みの送信 raw data を取得する。
 *
 * @return uint32_t 送信 raw data
 */
uint32_t InfraredRemote::getTxData(void) const { return txData_; }

/**
 * @brief デバッグ出力設定を取得する。
 *
 * @return true デバッグ有効
 * @return false デバッグ無効
 */
bool InfraredRemote::isDebug(void) const { return debug_; }

#endif
