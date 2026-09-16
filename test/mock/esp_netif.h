#pragma once
#include <cstdint>
#include <vector>
#include <array>
using esp_err_t=int;
constexpr int ESP_OK=0;
constexpr int ESP_IPADDR_TYPE_V4=0;
enum esp_netif_dns_type_t {ESP_NETIF_DNS_MAIN,ESP_NETIF_DNS_BACKUP,ESP_NETIF_DNS_FALLBACK};
struct esp_netif_t {};
struct esp_netif_dns_info_t {struct {int type;struct {struct {uint32_t addr;} ip4;} u_addr;} ip;};
namespace FakeNetif {
struct DnsCall {esp_netif_dns_type_t type;uint32_t address;};
inline std::vector<DnsCall>& dnsCalls() {static std::vector<DnsCall> calls;return calls;}
inline std::array<uint32_t,3>& servers() {static std::array<uint32_t,3> value{};return value;}
inline bool& inTcpip() {static bool value=false;return value;}
inline bool& unsafeDnsCall() {static bool value=false;return value;}
inline bool& execFails() {static bool value=false;return value;}
inline void reset() {dnsCalls().clear();servers()={};inTcpip()=false;unsafeDnsCall()=false;execFails()=false;}
}
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) {static esp_netif_t netif;return &netif;}
inline esp_err_t esp_netif_set_dns_info(esp_netif_t*,esp_netif_dns_type_t type,const esp_netif_dns_info_t* dns) {
  // ESP-IDF 4.4.7はゼロDNSを無効な引数として拒否する。
  if(!dns || !dns->ip.u_addr.ip4.addr)return -1;
  FakeNetif::dnsCalls().push_back({type,dns->ip.u_addr.ip4.addr});
  FakeNetif::servers()[type]=dns->ip.u_addr.ip4.addr;return ESP_OK;
}
using esp_netif_callback_fn=esp_err_t (*)(void*);
inline esp_err_t esp_netif_tcpip_exec(esp_netif_callback_fn fn,void* ctx) {
  if(FakeNetif::execFails())return -1;
  FakeNetif::inTcpip()=true;const auto result=fn(ctx);FakeNetif::inTcpip()=false;return result;
}
