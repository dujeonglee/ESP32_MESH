#include "routing.h"

static struct routing_entry routing_gw;
static struct routing_entry routing_tables[MAX_ROUTING_ENTRIES];
static uint8_t running = 0;

static struct netif *sta_interface = NULL;
static struct netif *ap_interface = NULL;

void update_routing(const uint32_t dest, const uint32_t next, const uint8_t out_if)
{
    uint8_t i = 0;
    if (out_if == OUT_IFACE_INAVLID)
    {
        printf("Routing update with invalid interface\n");
        return;
    }
    for (i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == dest)
        {
            routing_tables[i].next = next;
            routing_tables[i].out_if = out_if;
            printf("%hhu.%hhu.%hhu.%hhu -> %hhu.%hhu.%hhu.%hhu : %s\n",
                   ((uint8_t *)&dest)[0],
                   ((uint8_t *)&dest)[1],
                   ((uint8_t *)&dest)[2],
                   ((uint8_t *)&dest)[3],
                   ((uint8_t *)&next)[0],
                   ((uint8_t *)&next)[1],
                   ((uint8_t *)&next)[2],
                   ((uint8_t *)&next)[3],
                   (out_if == OUT_IFACE_STA ? "STA" : "AP"));
            return;
        }
    }
    for (i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].out_if == OUT_IFACE_INAVLID)
        {
            routing_tables[i].dest = dest;
            routing_tables[i].next = next;
            routing_tables[i].out_if = out_if;
            printf("%hhu.%hhu.%hhu.%hhu -> %hhu.%hhu.%hhu.%hhu : %s\n",
                   ((uint8_t *)&dest)[0],
                   ((uint8_t *)&dest)[1],
                   ((uint8_t *)&dest)[2],
                   ((uint8_t *)&dest)[3],
                   ((uint8_t *)&next)[0],
                   ((uint8_t *)&next)[1],
                   ((uint8_t *)&next)[2],
                   ((uint8_t *)&next)[3],
                   (out_if == OUT_IFACE_STA ? "STA" : "AP"));
            return;
        }
    }
}

void remove_routing(const uint32_t dest)
{
    uint8_t i = 0;
    for (i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == dest)
        {
            routing_tables[i].out_if = OUT_IFACE_INAVLID;
            return;
        }
    }
}

void update_gw(const uint32_t next, const uint8_t out_if)
{
    if (out_if == OUT_IFACE_INAVLID)
    {
        printf("Routing update with invalid interface\n");
        return;
    }
    printf("default gw -> %hhu.%hhu.%hhu.%hhu : %s\n",
           ((uint8_t *)&next)[0],
           ((uint8_t *)&next)[1],
           ((uint8_t *)&next)[2],
           ((uint8_t *)&next)[3],
           (out_if == OUT_IFACE_STA ? "STA" : "AP"));

    routing_gw.dest = 0;
    routing_gw.next = next;
    routing_gw.out_if = out_if;
}

void remove_gw()
{
    routing_gw.out_if = OUT_IFACE_INAVLID;
}

void start_routing(const uint32_t gateway, const uint8_t out_if)
{
    register_routing_function(mesh_ip4_forward);
    for (uint8_t i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        routing_tables[i].dest = 0;
        routing_tables[i].next = 0;
        routing_tables[i].out_if = OUT_IFACE_INAVLID;
    }
    update_gw(gateway, out_if);
    running = 1;
}

void stop_routing()
{
    running = 0;
    remove_gw();
    for (uint8_t i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        routing_tables[i].dest = 0;
        routing_tables[i].next = 0;
        routing_tables[i].out_if = OUT_IFACE_INAVLID;
    }
    unregister_routing_function();
}

bool mesh_client(const uint32_t addr)
{
    for (uint8_t i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == addr && routing_tables[i].out_if != OUT_IFACE_INAVLID)
        {
            return true;
        }
    }
    return false;
}

void mesh_ip4_forward(struct pbuf *p, struct ip_hdr *iphdr, struct netif *inp)
{
    struct netif *netif = NULL;
    char ifname[3];
    if (sta_interface == NULL)
    {
        ifname[0] = 's';
        ifname[1] = 't';
        ifname[2] = '0';
        for (; ifname[2] <= '9'; ifname[2]++)
        {
            sta_interface = netif_find(ifname);
            if (sta_interface)
            {
                break;
            }
        }
    }
    if (ap_interface == NULL)
    {
        ifname[0] = 'a';
        ifname[1] = 'p';
        ifname[2] = '0';
        for (; ifname[2] <= '9'; ifname[2]++)
        {
            ap_interface = netif_find(ifname);
            if (ap_interface)
            {
                break;
            }
        }
    }
    if ((ip4_addr_isbroadcast_u32(iphdr->dest.addr, inp)) || ((iphdr->dest.addr & PP_HTONL(0xf0000000UL)) == PP_HTONL(0xe0000000UL)))
    {
        return;
    }
/*
    printf("Packet from %s\n", (inp == sta_interface ? "STA" : (inp == ap_interface ? "AP" : "??")));
    printf("Length: %hu\n", ntohs(IPH_LEN(iphdr)));
    printf("Proto: %hu\n", IPH_PROTO(iphdr));
    printf("Src: %s\n", ip4addr_ntoa(&(iphdr->src)));
    printf("Dst: %s\n", ip4addr_ntoa(&(iphdr->dest)));
*/
    struct pbuf *new_p = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_RAM);
    if (!new_p)
    {
        printf("Buffer is not allocated\n");
    }
    if (pbuf_copy(new_p, p) == ERR_ARG)
    {
        printf("copy failed\n");
    }

    for (uint8_t i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == iphdr->dest.addr)
        {
            if (routing_tables[i].out_if == OUT_IFACE_AP)
            {
                //printf("To AP Interface\n");
                netif = ap_interface;
            }
            else if (routing_tables[i].out_if == OUT_IFACE_STA)
            {
                //printf("To STA Interface\n");
                netif = sta_interface;
            }
            else
            {
                netif = NULL;
            }
            if (netif->mtu && (new_p->tot_len > netif->mtu))
            {
                if ((IPH_OFFSET(iphdr) & PP_NTOHS(IP_DF)) == 0)
                {
                    ip4_frag(new_p, netif, (ip4_addr_t *)&routing_tables[i].next);
                    pbuf_free(new_p);
                    return;
                }
                pbuf_free(new_p);
                return;
            }
            //printf("Forwarded\n");
            netif->output(netif, new_p, (ip4_addr_t *)&routing_tables[i].next);
            pbuf_free(new_p);
            return;
        }
    }
    //printf("To Default GW\n");
    if (sta_interface->mtu && (p->tot_len > sta_interface->mtu))
    {
        if ((IPH_OFFSET(iphdr) & PP_NTOHS(IP_DF)) == 0)
        {
            ip4_frag(new_p, sta_interface, (ip4_addr_t *)&routing_gw.next);
            pbuf_free(new_p);
            return;
        }
        pbuf_free(new_p);
        return;
    }
    sta_interface->output(sta_interface, new_p, (ip4_addr_t *)&routing_gw.next);
    pbuf_free(new_p);
    return;
}
