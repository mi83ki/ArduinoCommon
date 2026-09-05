#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

extern uint32_t fakeMillis;

using byte = uint8_t;

static const uint8_t HEX = 16;

class String {
 public:
  String() = default;
  String(const char* value) : _value(value == nullptr ? "" : value) {}
  String(const char* value, unsigned int length)
      : _value(value == nullptr ? "" : std::string(value, length)) {}
  String(const std::string& value) : _value(value) {}

  String(unsigned long value, uint8_t base) {
    char buffer[32];
    if (base == HEX) {
      std::snprintf(buffer, sizeof(buffer), "%lx", value);
    } else {
      std::snprintf(buffer, sizeof(buffer), "%lu", value);
    }
    _value = buffer;
  }

  template <typename T,
            typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  String(T value) : _value(std::to_string(value)) {}

  const char* c_str() const { return _value.c_str(); }
  size_t length() const { return _value.length(); }
  bool isEmpty() const { return _value.empty(); }

  void concat(const String& value) { _value += value._value; }
  void concat(const char* value) { _value += value == nullptr ? "" : value; }

  String& operator+=(const String& value) {
    _value += value._value;
    return *this;
  }

  bool operator==(const String& value) const { return _value == value._value; }
  bool operator==(const char* value) const {
    return _value == (value == nullptr ? "" : value);
  }
  bool operator!=(const String& value) const { return !(*this == value); }
  bool operator!=(const char* value) const { return !(*this == value); }

  friend String operator+(const String& lhs, const String& rhs) {
    return String(lhs._value + rhs._value);
  }
  friend String operator+(const char* lhs, const String& rhs) {
    return String(std::string(lhs == nullptr ? "" : lhs) + rhs._value);
  }

 private:
  std::string _value;
};

inline uint32_t millis() { return fakeMillis; }
inline uint32_t micros() { return fakeMillis * 1000; }
inline void delay(unsigned long milliseconds) {
  fakeMillis += static_cast<uint32_t>(milliseconds);
}
inline void randomSeed(unsigned long) {}
inline long random(long maximum) { return maximum > 0 ? maximum / 2 : 0; }

#define F(value) (value)
#define RTC_DATA_ATTR
