#include <Arduino.h>

#include <Timer.h>

// 1000msごとに発火するタイマーを作成
Timer timer = Timer(1000);

void setup()
{
  Serial.begin(115200);
  Serial.println("Start example of Timer");
}

void loop()
{
  if (timer.isCycleTime())
  {
    // 1000msごとにデバイス時間を表示
    Serial.println(timer.getGlobalTime());
  }
}