#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

extern uint32_t fakeMillis;

class String {
 public:
  String() = default;
  String(const char* value) : _value(value == nullptr ? "" : value) {}
  String(const std::string& value) : _value(value) {}

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
inline void delay(unsigned long milliseconds) {
  fakeMillis += static_cast<uint32_t>(milliseconds);
}

#define F(value) (value)
#define RTC_DATA_ATTR
