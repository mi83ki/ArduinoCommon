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
#include <WiFiMulti.h>

#include <vector>

#include "Log.h"

/** WiFiの接続確立を待つ時間[ms] */
#define WIFI_TRY_WAIT (5000)
/** RTC情報を使用したWiFi接続の確立を待つ時間[ms] */
#define WIFI_FAST_CONNECT_WAIT (2000)
/** 通常SSIDへのWiFi接続の確立を待つ時間[ms] */
#define WIFI_PRIMARY_CONNECT_WAIT (3000)
/** WiFi再接続に失敗した後の再試行間隔[ms] */
#define WIFI_RECONNECT_INTERVAL (10000)

class WiFiESP32 {
 public:
  WiFiESP32(const char *, const char *);
  ~WiFiESP32();
  bool addAP(const char *, const char *);
  bool setStaticIp(const char *, const char *, const char *);
  bool begin(void);
  bool isConnected(void);
  bool healthCheck(void);

 private:
  struct WiFiCredential {
    String ssid;
    String password;
    bool primary;
  };

  bool connectWiFi(void);
  bool connectWithCredential(const WiFiCredential &, uint32_t, int32_t = 0,
                             const uint8_t * = nullptr);
  bool connectFromRtc(void);
  bool connectPrimary(void);
  bool connectFallback(void);
  bool configureNetwork(const WiFiCredential &);
  bool enableDhcp(void);
  const WiFiCredential *findCredential(const char *) const;
  void saveConnectedAccessPoint(void);
  void logConnectionResult(uint32_t) const;
  void disconnectWiFi(void);

  std::vector<WiFiCredential> _credentials;
  WiFiMulti _wifiMulti;
  IPAddress _staticIp;
  IPAddress _gateway;
  IPAddress _subnet;
  IPAddress _clientIp;
  uint32_t _lastFailedAttemptMs;
  bool _staticIpEnabled;
  bool _hasFailedAttempt;
};
