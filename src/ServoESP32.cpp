/**
 * @file ServoESP32.cpp
 * @brief ESP32用RCサーボ制御クラス
 * @author Tatsuya Miyazaki
 * @date 2023/3/19
 *
 * @details サーボの角速度を指定可能なRCサーボを制御するクラス
 * @note ESP32のledcを使用
 */

#include <Arduino.h>

#include "ServoESP32.h"

/** 角度[deg]（97.28分解能 / 180deg） */
const float ServoESP32::DEG_TO_PULSE = 0.540444444f;
/** -90degのパルス値 */
const float ServoESP32::PULSE_OFFSET = 25.6f;

/**
 * @brief Construct a new Servo E S P 3 2:: Servo E S P 3 2 object
 *
 * @param ch 使用するledcチャンネル
 * @param pin RCサーボのPWM入力に接続しているピン（0,2,4,12,13,14,15,25,26,27,32,33）
 */
ServoESP32::ServoESP32(uint8_t ch, uint8_t pin)
{
  _ch = ch;
  _pin = pin;
  _pulse = 0;
  _timer = new Timer(10);
  _angVel = 90.0f;
  _delta = new LinearChanger(10, _angVel, _angVel);
  ledcSetup(_ch, 50, 10); // 50 Hz 10bit resolution
  ledcAttachPin(_pin, _ch);
  ledcWrite(_ch, 0);
}

/**
 * @brief Construct a new Servo E S P 3 2:: Servo E S P 3 2 object
 *
 * @param ch 使用するledcチャンネル
 * @param pin RCサーボのPWM入力に接続しているピン（0,2,4,12,13,14,15,25,26,27,32,33）
 * @param initAngle 初期角度[deg]
 * @param angVel 回転角速度[deg/s]
 */
ServoESP32::ServoESP32(uint8_t ch, uint8_t pin, float initAngle, float angVel)
{
  _ch = ch;
  _pin = pin;
  _pulse = 0;
  _timer = new Timer(10);
  _angVel = angVel;
  _delta = new LinearChanger(10, _angVel, _angVel);
  ledcSetup(_ch, 50, 10); // 50 Hz 10bit resolution
  ledcAttachPin(_pin, _ch);
  ledcWrite(_ch, 0);
  setServoAngle(initAngle);
}

/**
 * @brief コピーコンストラクタ
 *
 * @param other
 */
ServoESP32::ServoESP32(const ServoESP32& other)
{
  _ch = other._ch;
  _pin = other._pin;
  _pulse = other._pulse;
  _timer = new Timer(other._timer->getCycleTime());
  _angVel = other._angVel;
  _delta = new LinearChanger(other._delta->getCycleTime(), _angVel, _angVel);
  ledcSetup(_ch, 50, 10); // 50 Hz 10bit resolution
  ledcAttachPin(_pin, _ch);
  ledcWrite(_ch, 0);
}

/**
 * @brief Destroy the Servo E S P 3 2:: Servo E S P 3 2 object
 */
ServoESP32::~ServoESP32()
{
  ledcDetachPin(_pin);
  delete _timer;
  delete _delta;
}

/**
 * @brief 角度を-90deg～90degの範囲に調整する
 *
 * @param angle 角度
 * @return float 調整後角度
 */
float ServoESP32::checkAngle(float angle)
{
  if (angle > 90.0f)
    angle = 90.0f;
  if (angle < -90.0f)
    angle = -90.0f;
  return angle;
}

/**
 * @brief 指定した角度にRCサーボを制御する
 *
 * @param angle 指定した角度(-90～90deg)
 */
float ServoESP32::setServoAngle(float angle)
{
  angle = moveServo(angle);
  _delta->setPresent(angle);
  _delta->setTarget(angle);
  return angle;
}

/**
 * @brief 指定した角度にRCサーボを動かす
 *
 * @param angle 指定した角度(-90～90deg)
 */
float ServoESP32::moveServo(float angle)
{
  angle = checkAngle(angle);
  _pulse = PULSE_OFFSET + DEG_TO_PULSE * (angle + 90.0f) + 0.5f;
  ledcWrite(_ch, _pulse);
  // Serial.println("serServoAngle(): angle = " + String(angle) + ", pulse = " + String(_pulse));
  return angle;
}

/**
 * @brief RCサーボの目標角度を設定する
 *
 * @param angle 目標角度[deg]
 */
void ServoESP32::setTargetAngle(float angle)
{
  angle = checkAngle(angle);
  _delta->setTarget(angle);
}

/**
 * @brief RCサーボの目標角度を設定する
 *
 * @param angle 目標角度[deg]
 */
void ServoESP32::setTargetAngle(float angle, float angVel)
{
  // Serial.println("setTargetAngle(): " + String(angle) + ", " + String(angVel));
  if (angVel != 0)
  {
    setAngularVelocity(angVel);
  }
  setTargetAngle(angle);
}

/**
 * @brief 目標角度を取得する
 * 
 * @return float 目標角度[deg]
 */
float ServoESP32::getTargetAngle(void)
{
  return _delta->getTarget();
}

/**
 * 指示が目標角度になったかどうか
 * @returns True:目標角度になった、False:目標角度にになっていない
 */
bool ServoESP32::isTargetAngle(void)
{
  return _delta->isTarget();
}

/**
 * @brief 回転角速度を変更する
 *
 * @param angVel 回転角速度[deg/s]
 */
void ServoESP32::setAngularVelocity(float angVel)
{
  _delta->setIncrease(angVel);
  _delta->setDecrease(angVel);
}

/**
 * @brief サーボを駆動する（一定時間毎に実行必要）
 */
void ServoESP32::loop(void)
{
  if (_timer->isCycleTime())
  {
    // 現在角度が目標角度に達していない場合
    if (!isTargetAngle())
    {
      _delta->update();
      moveServo(_delta->getPresent());
    }
  }
}
