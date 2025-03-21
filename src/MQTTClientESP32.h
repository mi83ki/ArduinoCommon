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

#include <WiFi.h>

#if __has_include(<PubSubClient.h>)
#include <PubSubClient.h>
#else
#warning "PubSubClient.h が見つかりません。MQTT 機能は無効化されます。"
// 必要に応じて、MQTT機能を無効化するためのマクロを定義する
#define MQTT_FEATURE_DISABLED
#endif

#ifndef MQTT_FEATURE_DISABLED

/** MQTTの接続リトライインターバル[ms] */
#define MQTT_RECONNECT_INTERVAL (5000)

class MQTTClientESP32
{
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

#endif