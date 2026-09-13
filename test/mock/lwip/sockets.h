#pragma once
#include <map>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/time.h>
constexpr int AF_INET=2,SOL_SOCKET=1,SO_RCVTIMEO=20;
using socklen_t=unsigned int;
struct in_addr {uint32_t s_addr;};
struct sockaddr {uint16_t sa_family;char data[14];};
struct sockaddr_in {uint16_t sin_family,sin_port;in_addr sin_addr;char zero[8];};
namespace FakeSockets {
inline std::map<int,std::string>& destinations() {static std::map<int,std::string> value;return value;}
inline uint32_t address(const char* s) {
  unsigned a,b,c,d;if(std::sscanf(s,"%u.%u.%u.%u",&a,&b,&c,&d)!=4)return 0;
  return a|(b<<8)|(c<<16)|(d<<24);
}
}
inline int fakeGetsockname(int socket,sockaddr* addr,socklen_t* size) {
  auto it=FakeSockets::destinations().find(socket);if(it==FakeSockets::destinations().end())return -1;
  sockaddr_in result{};result.sin_family=AF_INET;result.sin_addr.s_addr=FakeSockets::address(it->second.c_str());
  std::memcpy(addr,&result,sizeof(result));*size=sizeof(result);return 0;
}
inline int fakeSetsockopt(int,int,int,const void*,socklen_t) {return 0;}
#define getsockname fakeGetsockname
#define setsockopt fakeSetsockopt
