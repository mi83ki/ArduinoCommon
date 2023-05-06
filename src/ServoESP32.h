/**
 * @file ServoESP32.h
 * @brief ESP32用RCサーボ制御クラス
 * @author Tatsuya Miyazaki
 * @date 2023/3/19
 *
 * @details サーボの角速度を指定可能なRCサーボを制御するクラス
 * @note ESP32のledcを使用
 */

#pragma once

#include <Arduino.h>

#include "LinearChanger.h"
#include "Timer.h"

class ServoESP32
{
public:
  ServoESP32(uint8_t, uint8_t);
  ServoESP32(uint8_t, uint8_t, float, float);
  ServoESP32(const ServoESP32& other);
  ~ServoESP32();
  float setServoAngle(float);
  void setTargetAngle(float);
  void setTargetAngle(float, float);
  float getTargetAngle(void);
  float getPresentAngle(void);
  bool isTargetAngle(void);
  void setAngularVelocity(float);
  void loop(void);

  // 代入演算子
  ServoESP32& operator=(const ServoESP32& other)
  {
    if (this != &other)
    {
      _ch = other._ch;
      _pin = other._pin;
      _pulse = other._pulse;
      delete _timer;
      _timer = new Timer(other._timer->getCycleTime());
      _angVel = other._angVel;
      delete _delta;
      _delta =
        new LinearChanger(other._delta->getCycleTime(), other._delta->getIncrease(), other._delta->getDecrease());
    }
    return *this;
  }

private:
  float checkAngle(float);
  float moveServo(float);
  /** 角度[deg]（97.28分解能 / 180deg） */
  static const float DEG_TO_PULSE;
  /** -90°のパルス値 */
  static const float PULSE_OFFSET;
  /** LEDCのチャンネル */
  uint8_t _ch;
  /** サーボPWM出力ピン */
  uint8_t _pin;
  /** PWM比較一致パルス */
  uint16_t _pulse;
  /** 回転角速度[deg/s] */
  float _angVel;
  /** タイマー */
  Timer* _timer;
  /** 加減速制御インスタンス */
  LinearChanger* _delta;
};
