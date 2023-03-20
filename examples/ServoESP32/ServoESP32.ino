#include <Arduino.h>
#include <ServoESP32.h>
#include <Timer.h>

// サーボの指示角度[deg]（-90deg～90deg）
float angle = 0.0f;
// サーボ指示角度の増減値
float delta = 90.0f;
// サーボの回転角速度[deg/s]
float speed = 90.0f;

// 1つめのRCサーボをledcチャンネル0でIO26に設定
ServoESP32 servo1 = ServoESP32(0, 26);
// 2つめのRCサーボをledcチャンネル1でIO27に設定
ServoESP32 servo2 = ServoESP32(0, 27);
// 繰り返し動作用のタイマーを作成
Timer timer = Timer(3000);

void setup()
{
  Serial.begin(115200);
  Serial.println("Start example of ServoESP32");
  delay(3000);
}

void loop()
{
  if (timer.isCycleTime())
  {
    // -90°～90°を繰り返す
    angle += delta;
    delta = (angle >= 90.0f) ? -delta : ((angle <= -90.0f) ? -delta : delta);
    speed = speed == 90.0f ? 45.0f : 90.0f;
    servo1.setTargetAngle(angle, speed);
    servo2.setTargetAngle(angle, speed);
    Serial.println("angle = " + String(angle) + ", speed = " + String(speed));
  }
  servo1.loop();
  servo2.loop();
}