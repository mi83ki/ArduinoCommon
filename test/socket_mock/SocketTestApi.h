#pragma once
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#include <winsock2.h>
#undef ERROR
using socklen_t = int;
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 0x800
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#endif
int lwip_socket(int, int, int);
int lwip_fcntl(int, int, int);
int lwip_connect(int, const sockaddr*, socklen_t);
int lwip_select(int, fd_set*, fd_set*, fd_set*, timeval*);
int lwip_getsockopt(int, int, int, void*, socklen_t*);
int lwip_setsockopt(int, int, int, const void*, socklen_t);
int lwip_close(int);
int lwip_recv(int, void*, std::size_t, int);
int lwip_send(int, const void*, std::size_t, int);
std::uint32_t millis();
constexpr int WL_CONNECTED = 3;
struct FakeWiFi { int status() const; };
extern FakeWiFi WiFi;
