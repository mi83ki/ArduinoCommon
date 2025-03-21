#include <Arduino.h>

#include <Singleton.h>

class TestClass : public Singleton<TestClass>
{
  friend class Singleton<PropertyManager>;

public:
  TestClass() : time_(millis()) {}
  ~TestClass() {}
  uint32_t getTime() { return time_; }

private:
  uint32_t time_;
};

void setup()
{
  logger.info("Start example of Singleton");
  TestClass& instance = TestClass::getInstance();
  logger.debug("TestClass::getTime(): " + String(instance.getTime()));
  delay(3000);
}

void loop()
{
  logger.debug("TestClass::getTime(): " + String(TestClass::getInstance()->getTime()));
  delay(1000);
}
