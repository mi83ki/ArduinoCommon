#pragma once
#include <array>
#include <string>
#include <vector>
#include <cstdint>

namespace ArduinoCommon {
using IPv4=std::array<uint8_t,4>;
struct WiFiProfile {
  uint8_t id=0;
  std::string ssid,password;
  bool staticIp=false;
  IPv4 ip{},gateway{},mask{},dns1{},dns2{};
};
struct WiFiValidationError {std::string field,code;};
enum class WiFiPasswordAction {Keep,Replace,Clear};
class WiFiProfileValidator {
 public:
  static bool validUtf8(const std::string&);
  static bool validPassword(const std::string&);
  static bool validStaticIp(const WiFiProfile&,bool requiresDns);
  static std::vector<WiFiValidationError> validate(const std::vector<WiFiProfile>&,uint8_t primaryId,bool ready);
  static bool applyPassword(WiFiProfile&,WiFiPasswordAction,const std::string& value,bool open,
                            const std::vector<WiFiProfile>& existing);
};
}
