#ifndef DJMESH_ARP_PROXY_H
#define DJMESH_ARP_PROXY_H
#include <stdint.h>
#include "netif/ethernet.h"
#include "netif/raw_ethernet.h"
#include "netif/etharp.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void start_arp_proxy();
    void stop_arp_proxy();
    err_t etharp_raw(struct netif *netif, const struct eth_addr *ethsrc_addr,
                 const struct eth_addr *ethdst_addr,
                 const struct eth_addr *hwsrc_addr, const ip4_addr_t *ipsrc_addr,
                 const struct eth_addr *hwdst_addr, const ip4_addr_t *ipdst_addr,
                 const u16_t opcode);

#ifdef __cplusplus
}
#endif

#endif
