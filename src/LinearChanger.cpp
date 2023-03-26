/**
 * @file LinearChanger.cpp
 * @brief 値を線形で増減させる
 * @author Tatsuya Miyazaki
 * @date 2023/3/19
 *
 * @details 例えば速度を徐々に上げたい場合などに使用するクラス
 * @note 浮動小数点演算ではなく、固定小数点演算(fix.hpp)を行います（処理の高速化のため）
 */

#include <Arduino.h>

#include "LinearChanger.h"

/**
 * コンストラクタ
 * @param time 更新周期[ms]
 * @param increase 加速度
 * @param decrease 減速度
 */
LinearChanger::LinearChanger(uint16_t time, float increase, float decrease)
{
  _present = 0;
  _target = 0;
  _cycleTime = INT_TO_FIX(time) / 1000;
  _increase = FLOAT_TO_FIX(increase);
  _decrease = FLOAT_TO_FIX(decrease);
}

/**
 * @brief Destroy the Linear Increaser:: Linear Increaser object
 */
LinearChanger::~LinearChanger()
{
  ;
}

/**
 * @brief 増加が減少かを判断し、増減量を返す
 *
 * @return fix 増減量を返す
 */
fix LinearChanger::getDelta(void)
{
  if (_present == _target)
  {
    return (0);
  }
  else if (_target == 0)
  {
    return (_decrease);
  }
  else if (_target > 0)
  {
    if (_present < _target)
    {
      return (_increase);
    }
    else
    {
      return (_decrease);
    }
  }
  else
  {
    if (_present > _target)
    {
      return (_increase);
    }
    else
    {
      return (_decrease);
    }
  }
  return (0);
}

/**
 * @brief 値を増減させる（コンストラクタで設定した実行周期で実行すること）
 *
 * @return fix 更新した値
 */
fix LinearChanger::updateFix(void)
{
  fix increase = getDelta();
  if (_present < _target)
  {
    _present += FIX_MUL(increase, _cycleTime);
    if (_present > _target)
    {
      _present = _target;
    }
  }
  else if (_present > _target)
  {
    _present -= FIX_MUL(increase, _cycleTime);
    if (_present < _target)
    {
      _present = _target;
    }
  }
  // Serial.println("updateFix: " + String(_target) + ", " + String(_present) + ", " + String(_increase));
  return (_present);
}

/**
 * @brief 値を増減させる（コンストラクタで設定した実行周期で実行すること）
 *
 * @return fix 更新した値
 */
float LinearChanger::update(void)
{
  return FIX_TO_FLOAT(updateFix());
}

/**
 * @brief 目標値を設定する
 *
 * @param target 目標値
 */
void LinearChanger::setTargetFix(fix target)
{
  _target = target;
}

/**
 * @brief 目標値を設定する
 *
 * @param target 目標値
 */
void LinearChanger::setTarget(float target)
{
  setTargetFix(FLOAT_TO_FIX(target));
}

/**
 * @brief 目標値を取得する
 *
 * @return fix
 */
fix LinearChanger::getTargetFix(void)
{
  return (_target);
}

/**
 * @brief 目標値を取得する
 *
 * @return float
 */
float LinearChanger::getTarget(void)
{
  return FIX_TO_FLOAT(getTargetFix());
}

/**
 * @brief 現在値を取得する
 *
 * @return fix
 */
fix LinearChanger::getPresentFix(void)
{
  return (_present);
}

/**
 * @brief 現在値を取得する
 *
 * @return float
 */
float LinearChanger::getPresent(void)
{
  return FIX_TO_FLOAT(getPresentFix());
}

/**
 * @brief 現在値を設定する
 *
 * @param present 現在値
 */
void LinearChanger::setPresent(float present)
{
  _present = FLOAT_TO_FIX(present);
}

/**
 * @brief 目標値に到達したかどうか
 *
 * @return true 目標値に到達
 * @return false 目標値に未達
 */
bool LinearChanger::isTarget(void)
{
  return _present == _target;
}

/**
 * @brief 単位時間当たりの増加量を設定する
 *
 * @param increase 単位時間当たりの増加量
 */
void LinearChanger::setIncrease(float increase)
{
  _increase = FLOAT_TO_FIX(increase);
}

/**
 * @brief 単位時間当たりの増加量を取得する
 *
 * @return float 単位時間当たりの増加量
 */
float LinearChanger::getIncrease(void)
{
  return FIX_TO_FLOAT(_increase);
}

/**
 * @brief 単位時間当たりの減少量を設定する
 *
 * @param decrease 単位時間当たりの減少量
 */
void LinearChanger::setDecrease(float decrease)
{
  _decrease = FLOAT_TO_FIX(decrease);
}

/**
 * @brief 単位時間当たりの減少量を取得する
 *
 * @return float 単位時間当たりの減少量
 */
float LinearChanger::getDecrease(void)
{
  return FIX_TO_FLOAT(_decrease);
}

/**
 * @brief 更新周期[ms]を取得する
 *
 * @return uint16_t 更新周期[ms]
 */
uint16_t LinearChanger::getCycleTime(void)
{
  return FIX_TO_INT(_cycleTime);
}
