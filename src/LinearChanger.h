/**
 * @file LinearChanger.h
 * @brief 値を線形で増減させる
 * @author Tatsuya Miyazaki
 * @date 2023/3/19
 *
 * @details 例えば速度を徐々に上げたい場合などに使用するクラス
 * @note 浮動小数点演算ではなく、固定小数点演算(fix.hpp)を行います（処理の高速化のため）
 */

#pragma once

#include <Arduino.h>

#include "fix.hpp"

class LinearChanger
{
public:
  LinearChanger(uint16_t, float, float);
  ~LinearChanger();
  fix updateFix(void);
  float update(void);
  void setTargetFix(fix);
  void setTarget(float);
  fix getTargetFix(void);
  float getTarget(void);
  fix getPresentFix(void);
  float getPresent(void);
  bool isTarget(void);
  void setIncrease(float);
  float getIncrease(void);
  void setDecrease(float);
  float getDecrease(void);
  uint16_t getCycleTime(void);

private:
  fix getDelta(void);

  /** 現在値 */
  fix _present;
  /** 目標値 */
  fix _target;
  /** 単位時間当たりの増加量 */
  fix _increase;
  /** 単位時間当たりの減少量 */
  fix _decrease;
  /** 更新周期[ms] */
  fix _cycleTime;
};
