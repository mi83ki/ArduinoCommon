#pragma once
#include <cstdint>
#include <vector>
using esp_err_t=int;
constexpr int ESP_OK=0;
constexpr int ESP_IPADDR_TYPE_V4=0;
enum esp_netif_dns_type_t {ESP_NETIF_DNS_MAIN,ESP_NETIF_DNS_BACKUP,ESP_NETIF_DNS_FALLBACK};
struct esp_netif_t {};
struct esp_netif_dns_info_t {struct {int type;struct {struct {uint32_t addr;} ip4;} u_addr;} ip;};
namespace FakeNetif {
struct DnsCall {esp_netif_dns_type_t type;uint32_t address;};
inline std::vector<DnsCall>& dnsCalls() {static std::vector<DnsCall> calls;return calls;}
}
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*) {static esp_netif_t netif;return &netif;}
inline esp_err_t esp_netif_set_dns_info(esp_netif_t*,esp_netif_dns_type_t type,const esp_netif_dns_info_t* dns) {
  FakeNetif::dnsCalls().push_back({type,dns->ip.u_addr.ip4.addr});return ESP_OK;
}
