#ifndef DJMESH_ROUTING_H
#define DJMESH_ROUTING_H
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_frag.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    #define OUT_IFACE_INAVLID  (0)
    #define OUT_IFACE_STA      (1)
    #define OUT_IFACE_AP       (2)

    #define MAX_ROUTING_ENTRIES (30)
    struct routing_entry
    {
        uint32_t dest;
        uint32_t next;
        uint8_t mac[6];
        uint8_t out_if;
    };

    void update_routing(const uint32_t dest, const uint32_t next, const uint8_t out_if);
    void remove_routing(const uint32_t dest, const uint8_t* hw);
    void update_gw(const uint32_t gw, const uint8_t out_if);
    void remove_gw();

    void start_routing(const uint32_t gw, const uint8_t out_if);
    void stop_routing();

    bool mesh_client(const uint32_t addr);

    void mesh_ip4_forward(struct pbuf *p, struct ip_hdr *iphdr, struct netif *inp);
#ifdef __cplusplus
}
#endif

#endif
