#pragma once
#include "sockets.h"
inline uint32_t inet_addr(const char* value) {return FakeSockets::address(value);}
