#include <Arduino.h>
#include <esp_system.h>
#include <Log.h>

#include "SleepHandler.h"

#define S_TO_US (1000000ULL)  // Conversion factor for micro seconds to seconds

/** Deepsleep用にRTCメモリ域に変数を確保 */
RTC_SLOW_ATTR int timerCount = 0;

/**
 * @brief スリープからの起動理由を出力する
 *
 */
void printWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      logger.info("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      logger.info("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      logger.info("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      logger.info("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      logger.info("Wakeup caused by ULP program");
      break;
    default:
      logger.info("Wakeup was not caused by deep sleep. wakeup_reason: " +
                  String(wakeup_reason));
      break;
  }
}

/**
 * @brief 指定された秒数だけスリープする
 *
 * @param sleepTimeSeconds スリープする秒数
 */
void sleepSeconds(uint64_t sleepTimeSeconds) {
  timerCount++;
  logger.info("timer_cont = " + String(timerCount));
  uint64_t sleepTimeUs = sleepTimeSeconds * S_TO_US;
  logger.info("sleepTimeUs = " + String(sleepTimeUs));
  esp_sleep_enable_timer_wakeup(sleepTimeUs);
  esp_deep_sleep_start();
}
