/**
 * @file Timer.h
 * @brief タイマークラス
 * @author Tatsuya Miyazaki
 * @date 2020/6/28
 *
 * @details 時間を計測したり、周期トリガを生成するクラス
 */

#pragma once

#include <Arduino.h>

class Timer
{
public:
  Timer();
  explicit Timer(uint16_t);
  void setCycleTime(uint16_t);
  uint16_t getCycleTime(void);
  bool isCycleTime(void);
  void startTimer(void);
  void stopTimer(void);
  uint32_t getTime(void);
  static uint32_t getGlobalTime(void) { return (millis()); }

private:
  /** 開始したときの時刻[ms] */
  uint32_t startTime;
  /** 停止したときの時刻[ms] */
  uint32_t stopTime;
  /** 前回の時間÷周期 */
  uint32_t pastTime;
  /** チェックする周期[ms] */
  uint16_t cycleTime;
};
