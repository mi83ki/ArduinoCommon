#pragma once
#include <map>
#include <string>
#include <cstring>
#include <cstdint>
#include <sys/time.h>
constexpr int AF_INET=2,AF_INET6=10,SOL_SOCKET=1,SO_RCVTIMEO=20;
using socklen_t=unsigned int;
struct in_addr {uint32_t s_addr;};
struct sockaddr {uint16_t sa_family;char data[14];};
struct sockaddr_in {uint16_t sin_family,sin_port;in_addr sin_addr;char zero[8];};
struct in6_addr {uint8_t s6_addr[16];};
struct sockaddr_in6 {uint16_t sin6_family,sin6_port;uint32_t sin6_flowinfo;in6_addr sin6_addr;uint32_t sin6_scope_id;};
struct sockaddr_storage {uint16_t ss_family;char data[30];};
namespace FakeSockets {
inline std::map<int,std::string>& destinations() {static std::map<int,std::string> value;return value;}
inline std::map<int,socklen_t>& reportedSizes() {static std::map<int,socklen_t> value;return value;}
inline uint32_t address(const char* s) {
  unsigned a,b,c,d;if(std::sscanf(s,"%u.%u.%u.%u",&a,&b,&c,&d)!=4)return 0;
  return a|(b<<8)|(c<<16)|(d<<24);
}
}
inline int fakeGetsockname(int socket,sockaddr* addr,socklen_t* size) {
  auto it=FakeSockets::destinations().find(socket);if(it==FakeSockets::destinations().end())return -1;
  sockaddr_storage storage{};socklen_t actual;
  if(it->second.find(':')!=std::string::npos) {
    sockaddr_in6 result{};result.sin6_family=AF_INET6;
    if(it->second.compare(0,7,"::ffff:")==0) {
      result.sin6_addr.s6_addr[10]=255;result.sin6_addr.s6_addr[11]=255;
      const auto ipv4=FakeSockets::address(it->second.c_str()+7);
      std::memcpy(result.sin6_addr.s6_addr+12,&ipv4,4);
    } else {result.sin6_addr.s6_addr[0]=0x20;result.sin6_addr.s6_addr[1]=1;}
    actual=sizeof(result);std::memcpy(&storage,&result,actual);
  } else {
    sockaddr_in result{};result.sin_family=AF_INET;result.sin_addr.s_addr=FakeSockets::address(it->second.c_str());
    actual=sizeof(result);std::memcpy(&storage,&result,actual);
  }
  if(FakeSockets::reportedSizes().count(socket))actual=FakeSockets::reportedSizes()[socket];
  std::memcpy(addr,&storage,*size<actual?*size:actual);*size=actual;return 0;
}
inline int fakeSetsockopt(int,int,int,const void*,socklen_t) {return 0;}
#define getsockname fakeGetsockname
#define setsockopt fakeSetsockopt
