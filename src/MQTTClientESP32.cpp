/**
 * @file MQTTClientESP32.h
 * @brief ESP32用MQTT接続クラス
 * @author Tatsuya Miyazaki
 * @date 2024/8/19
 *
 * @details MQTTへの接続を管理するクラス
 */

#include "MQTTClientESP32.h"
#include "Log.h"

/**
 * @brief Construct a new MQTTClientESP32::MQTTClientESP32 object
 *
 * @param mqttHost MQTTブローカーのIPアドレス
 * @param mqttPort MQTTブローカーのポート番号
 * @param bufferSize MQTTバッファ―サイズ
 */
MQTTClientESP32::MQTTClientESP32(String mqttHost, uint16_t mqttPort,
                                 uint16_t bufferSize)
    : _mqttHost(mqttHost), _mqttPort(mqttPort), _lastReconnectAttempt(0),
      _wifiClient(WiFiClient()), _mqttClient(_wifiClient) {

  logger.info("Start example of MQTTClientESP32 " + _mqttHost + ":" +
              String(_mqttPort));
  _mqttClient.setServer(_mqttHost.c_str(), _mqttPort);
  if (bufferSize > 0) {
    _mqttClient.setBufferSize(bufferSize);
    logger.info("MQTTClientESP32.setBufferSize: " + String(bufferSize));
  }

  // ランダム関数の初期化
  randomSeed(micros());
  _clientId = "m5timercamera-" + String(random(0xffff), HEX);
  logger.info("MQTTClientESP32.clientId: " + _clientId);
}

/**
 * @brief Destroy the MQTTClientESP32::MQTTClientESP32 object
 *
 */
MQTTClientESP32::~MQTTClientESP32() {}

bool MQTTClientESP32::reconnect() {
  logger.info("MQTTClientESP32.reconnect(): start");
  if (_mqttClient.connect(_clientId.c_str())) {
    // // Once connected, publish an announcement...
    // _mqttClient.publish("outTopic","hello world");
    // // ... and resubscribe
    // _mqttClient.subscribe("inTopic");
  }
  return _mqttClient.connected();
}

/**
 * @brief 接続確認（再接続処理含む）
 *
 * @return true
 * @return false
 */
bool MQTTClientESP32::healthCheck(void) {
  if (!_mqttClient.connected()) {
    long now = millis();
    if (now - _lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
      _lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        _lastReconnectAttempt = 0;
        logger.info("MQTTClientESP32.healthCheck(): success to reconnect");
        return true;
      }
    }
  } else {
    // Client connected
    _mqttClient.loop();
    return true;
  }
  return false;
}

/**
 * @brief メッセージをPublishする
 *
 * @param topic トピック名
 * @param payload ペイロード
 * @return true
 * @return false
 */
bool MQTTClientESP32::publish(String topic, String payload) {
  return publish(topic, payload.c_str(), payload.length());
}

bool MQTTClientESP32::publish(String topic, const char *payload, int plength) {
  uint16_t mqttBufSize = _mqttClient.getBufferSize();
  uint16_t dataSize = MQTT_MAX_HEADER_SIZE + 2 + topic.length() + plength;
  if (dataSize > mqttBufSize) {
    // メッセージサイズがオーバーしているとき
    logger.warn("MQTT message too long. bufSize: " + String(mqttBufSize) +
                ", dataSize: " + String(dataSize));
    _mqttClient.beginPublish(topic.c_str(), plength, false);
    _mqttClient.print(payload);
    _mqttClient.endPublish();
    return true;
  }
  return _mqttClient.publish(topic.c_str(), payload, plength);
}

/**
 * @brief メッセージをSubscribeする
 *
 * @param topic
 * @return true
 * @return false
 */
bool MQTTClientESP32::subscribe(String topic) {
  return _mqttClient.subscribe(topic.c_str());
}
