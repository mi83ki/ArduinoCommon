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
#include <vector>
#include <functional>

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
  // コールバック関数の型定義
  using MessageCallback = std::function<void(String topic, String payload)>;

  MQTTClientESP32(String, uint16_t, uint16_t bufferSize = 0);
  ~MQTTClientESP32();
  PubSubClient *getMQTTClient(void) { return &_mqttClient; };
  bool healthCheck(void);
  bool publish(String topic, String payload, bool retained = false);
  bool publish(String topic, const char *payload, int plength, bool retained = false);
  bool subscribe(String topic);
  String getClientId(void) { return _clientId; };
  void onMessage(char *topic, byte *payload, unsigned int length);
  
  // コールバック関数を登録する関数
  void registOnMessageCallback(MessageCallback callback);

private:
  bool reconnect(void);

  uint16_t _lastReconnectAttempt;
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
  String _mqttHost;
  uint16_t _mqttPort;
  String _clientId;
  
  // コールバック関数のリスト
  std::vector<MessageCallback> _messageCallbacks;
  
  // subscribeしたトピックのリスト
  std::vector<String> _subscribedTopics;
};

#endif
