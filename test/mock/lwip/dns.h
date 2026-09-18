#pragma once
#include "esp_netif.h"
struct ip_addr_t {uint32_t addr;};
inline void dns_setserver(uint8_t index,const ip_addr_t* address) {
  if(!FakeNetif::inTcpip())FakeNetif::unsafeDnsCall()=true;
  const uint32_t value=address?address->addr:0;
  FakeNetif::servers().at(index)=value;
  FakeNetif::dnsCalls().push_back({static_cast<esp_netif_dns_type_t>(index),value});
}
