/**
 * @file WiFiESP32.h
 * @brief ESP32用WiFi制御クラス
 * @author Tatsuya Miyazaki
 * @date 2023/5/5
 *
 * @details WiFiへの接続を管理するクラス
 */

#pragma once

#include <Arduino.h>

#include <WiFi.h>

#include "Log.h"
#include "Timer.h"


/** WiFiの接続確立を待つ時間[ms] */
#define WIFI_TRY_WAIT (5000)
/** WiFi接続何回失敗したらあきらめるか */
#define WIFI_TRY_TIME (2)

class WiFiESP32
{
public:
  WiFiESP32(const char *, const char *);
  ~WiFiESP32();
  bool begin(void);
  bool isConnected(void);
  bool healthCheck(void);

private:
  bool connectWiFi(void);
  void disconnectWiFi(void);

private:
  const char *m_myWiFiSSID;
  const char *m_myWiFiPass;
  IPAddress m_clientIP;
};
