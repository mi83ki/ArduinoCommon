/**
 * @file Timer.cpp
 * @brief タイマークラス
 * @author Tatsuya Miyazaki
 * @date 2020/6/28
 *
 * @details 時間を計測したり、周期トリガを生成するクラス
 */

#include <Arduino.h>

#include "Timer.h"

/**
 * @brief Construct a new Timer:: Timer object
 *
 */
Timer::Timer() : startTime(millis()), stopTime(millis()), pastTime(0), cycleTime(0) {}

/**
 * @brief Construct a new Timer:: Timer object
 * 
 * @param time 周期[ms]
 */
Timer::Timer(uint16_t time) : startTime(millis()), stopTime(millis()), pastTime(0), cycleTime(time) {}

/**
 * @brief 周期を設定する
 * 
 * @param time 周期[ms]
 */
void Timer::setCycleTime(uint16_t time)
{
  cycleTime = time;
}

/**
 * @brief 周期[ms]を取得する
 *
 * @return uint16_t 周期[ms]
 */
uint16_t Timer::getCycleTime(void)
{
  return cycleTime;
}

/**
 * @brief 周期が来たことを知らせ、フラグをクリアする
 * 
 * @return true 周期がきた
 * @return false 周期でない
 */
bool Timer::isCycleTime(void)
{
  uint32_t temp = millis() / (uint32_t)cycleTime;
  uint32_t returnData = temp - pastTime;
  pastTime = temp;
  return (returnData > 0);
}

/**
 * @brief タイマーを開始する
 * 
 */
void Timer::startTimer(void)
{
  startTime = millis();
  stopTime = 0;
}

/**
 * @brief タイマーを停止する
 * 
 */
void Timer::stopTimer(void)
{
  stopTime = millis();
}

/**
 * @brief 経過時間[ms]を取得する
 * 
 * @return uint32_t 経過時間[ms]
 */
uint32_t Timer::getTime(void)
{
  if (stopTime == 0)
    return millis() - startTime;
  else
    return stopTime - startTime;
}
