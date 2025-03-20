/**
 * @file MQTTClientESP32.h
 * @brief ESP32用MQTT接続クラス
 * @author Tatsuya Miyazaki
 * @date 2024/8/19
 *
 * @details MQTTへの接続を管理するクラス
 */

#pragma once

#include <Arduino.h>

#include <PubSubClient.h>
#include <WiFi.h>

/** MQTTの接続リトライインターバル[ms] */
#define MQTT_RECONNECT_INTERVAL (5000)

class MQTTClientESP32 {
public:
  MQTTClientESP32(String, uint16_t, uint16_t bufferSize = 0);
  ~MQTTClientESP32();
  PubSubClient *getMQTTClient(void) { return &_mqttClient; };
  bool healthCheck(void);
  bool publish(String topic, String payload);
  bool publish(String topic, const char *payload, int plength);
  bool subscribe(String topic);
  String getClientId(void) { return _clientId; };

private:
  bool reconnect(void);

  uint16_t _lastReconnectAttempt;
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
  String _mqttHost;
  uint16_t _mqttPort;
  String _clientId;
};
