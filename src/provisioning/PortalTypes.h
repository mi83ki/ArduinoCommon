#pragma once
#include <string>

namespace ArduinoCommon {
enum class PortalMethod {Get,Post};
struct PortalRequest {PortalMethod method;std::string path,body;};
struct PortalResponse {int status=200;std::string body="{}";};
}
