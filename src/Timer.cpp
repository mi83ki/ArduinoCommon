/***********************************************************************/
/*                                                                     */
/*  FILE        :Timer.cpp                                            */
/*  DATE        :Jun 28, 2020                                          */
/*  DESCRIPTION :タイマークラス                                        */
/*                                                                     */
/*  This file is generated by Tatsuya Miyazaki                         */
/*                                                                     */
/***********************************************************************/

#include "Timer.h"

/***********************************************************************/
/*                           タイマー関数                              */
/*        注：millis()を使用する                                       */
/***********************************************************************/

// コンストラクタ
Timer::Timer()
{
  tempTimer = 0;
}

// コンストラクタ(周期[ms])
Timer::Timer(uint16_t time)
{
  pastTime = 0;
  cycleTime = time;
}

// 周期を設定する
void Timer::setCycleTime(uint16_t time)
{
  cycleTime = time;
}

// 周期が来たことを知らせ、フラグをクリアする（1:周期がきた、0:周期でない）
uint8_t Timer::isCycleTime(void)
{
  uint32_t temp = millis() / (uint32_t)cycleTime;
  uint32_t returnData = temp - pastTime;
  pastTime = temp;
  return (returnData);
}

// タイマーをクリアする
void Timer::startTimer(void)
{
  tempTimer = millis();
}

// タイマー値[ms]を取得する
uint32_t Timer::getTime(void)
{
  return (millis() - tempTimer);
}
