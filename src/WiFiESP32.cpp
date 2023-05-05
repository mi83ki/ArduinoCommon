/**
 * @file WiFiESP32.h
 * @brief ESP32用WiFi制御クラス
 * @author Tatsuya Miyazaki
 * @date 2023/5/5
 *
 * @details WiFiへの接続を管理するクラス
 */

#include "WiFiESP32.h"

/**
 * @brief Construct a new Wi Fi E S P 3 2:: Wi Fi E S P 3 2 object
 *
 * @param SSID WiFiのSSID
 * @param PASS WiFiのパスワード
 */
WiFiESP32::WiFiESP32(const char *SSID, const char *PASS)
{
  m_myWiFiSSID = SSID;
  m_myWiFiPass = PASS;
}

/**
 * @brief Destroy the Wi Fi E S P 3 2:: Wi Fi E S P 3 2 object
 *
 */
WiFiESP32::~WiFiESP32() {}

/**
 * @brief WiFi接続開始処理
 *
 * @return true 接続成功
 * @return false 接続失敗
 */
bool WiFiESP32::begin(void)
{
  uint8_t cnt = 0;
  while (connectWiFi() == false)
  {
    disconnectWiFi();
    // 一定回数失敗したら終了する
    if (++cnt >= WIFI_TRY_TIME)
    {
      return (false);
    }
  }
  return (true);
}

/**
 * @brief WiFiへの接続処理
 *
 * @return true 接続成功
 * @return false 接続失敗
 */
bool WiFiESP32::connectWiFi(void)
{
  uint16_t tryTime = 0;

  logger.info("WiFi Connecting....");
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(m_myWiFiSSID, m_myWiFiPass);
  do
  {
    if (tryTime >= WIFI_TRY_WAIT)
    {
      // 一定接続できなかったらfalseを返して終了
      logger.error("WiFiESP32::connectWiFi(): WiFi booting failed.");
      return (false);
    }
    delay(500);
    tryTime += 500;
    logger.info(".");
  } while (WiFi.status() != WL_CONNECTED);
  String msg = "Connected. I am ";
  m_clientIP = WiFi.localIP();
  msg.concat(m_clientIP.toString());
  logger.info(msg);
  return (true);
}

/**
 * @brief WiFi切断処理
 * 
 */
void WiFiESP32::disconnectWiFi(void)
{
  WiFi.disconnect();
  logger.info("WiFi Disconnected.");
}

/**
 * @brief WiFiが接続されているかどうか
 * 
 * @return true 接続
 * @return false 切断
 */
bool WiFiESP32::isConnected(void)
{
  return (WiFi.status() == WL_CONNECTED);
}

/**
 * @brief WiFiの接続チェックし、切断している場合は再接続をトライする
 * 
 * @return true 接続
 * @return false 切断
 */
bool WiFiESP32::healthCheck(void)
{
  if (isConnected())
  {
    return (true);
  }
  else
  {
    disconnectWiFi();
    connectWiFi();
    if (isConnected())
    {
      return (true);
    }
    else
    {
      return (false);
    }
  }
}
