#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_trace.h"

#include "arp_proxy.h"
#include "routing.h"

static esp_timer_handle_t periodic_route_adv_timer;
static struct routing_entry routing_gw;
static struct routing_entry routing_tables[MAX_ROUTING_ENTRIES];
static uint8_t running = 0;

static char *TAG = "ROUTING";

static struct netif *sta_interface = NULL;
static struct netif *ap_interface = NULL;

static void periodic_route_adv_timer_callback(void *arg)
{
    //ESP_LOGI(TAG, "Route Adv\n");
}

void update_routing(const uint32_t dest, const uint32_t next, const uint8_t out_if)
{
    uint8_t i = 0;
    if (out_if == OUT_IFACE_INAVLID)
    {
        ESP_LOGE(TAG, "Routing update with invalid interface\n");
        return;
    }
    for (i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == dest)
        {
            routing_tables[i].next = next;
            routing_tables[i].out_if = out_if;
            ESP_LOGI(TAG, "Add/Update Route : %hhu.%hhu.%hhu.%hhu -> %hhu.%hhu.%hhu.%hhu : %s\n",
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
            ESP_LOGI(TAG, "Add/Update Route : %hhu.%hhu.%hhu.%hhu -> %hhu.%hhu.%hhu.%hhu : %s\n",
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

void remove_routing(const uint32_t dest, const uint8_t *hw)
{
    uint8_t i = 0;
    for (i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == dest)
        {
            ESP_LOGI(TAG, "Delete Route : %hhu.%hhu.%hhu.%hhu\n",
                     ((uint8_t *)&dest)[0],
                     ((uint8_t *)&dest)[1],
                     ((uint8_t *)&dest)[2],
                     ((uint8_t *)&dest)[3]);
            routing_tables[i].out_if = OUT_IFACE_INAVLID;
            // if(hw && sta_interface)
            // {
            //     ESP_LOGI(TAG, "Send GARP\n");
            //     struct eth_addr src;
            //     struct eth_addr dst;
            //     struct eth_addr arp_hw_src;
            //     struct eth_addr arp_hw_dst;
            //     src.addr[0] = hw[0];
            //     src.addr[1] = hw[1];
            //     src.addr[2] = hw[2];
            //     src.addr[3] = hw[3];
            //     src.addr[4] = hw[4];
            //     src.addr[5] = hw[5];
            //     dst.addr[0] = 0xff;
            //     dst.addr[1] = 0xff;
            //     dst.addr[2] = 0xff;
            //     dst.addr[3] = 0xff;
            //     dst.addr[4] = 0xff;
            //     dst.addr[5] = 0xff;
            //     arp_hw_src.addr[0] = hw[0];
            //     arp_hw_src.addr[1] = hw[1];
            //     arp_hw_src.addr[2] = hw[2];
            //     arp_hw_src.addr[3] = hw[3];
            //     arp_hw_src.addr[4] = hw[4];
            //     arp_hw_src.addr[5] = hw[5];
            //     arp_hw_dst.addr[0] = 0xff;
            //     arp_hw_dst.addr[1] = 0xff;
            //     arp_hw_dst.addr[2] = 0xff;
            //     arp_hw_dst.addr[3] = 0xff;
            //     arp_hw_dst.addr[4] = 0xff;
            //     arp_hw_dst.addr[5] = 0xff;
            //     if(ERR_OK != etharp_raw(sta_interface, &src, &dst, &arp_hw_src, &dest, &arp_hw_dst, &dest, ARP_REQUEST))
            //     {
            //         ESP_LOGE(TAG, "GARP is failed\n");
            //     }
            // }
            return;
        }
    }
}

void update_gw(const uint32_t next, const uint8_t out_if)
{
    if (out_if == OUT_IFACE_INAVLID)
    {
        ESP_LOGI(TAG, "Routing update with invalid interface\n");
        return;
    }
    ESP_LOGI(TAG, "default gw -> %hhu.%hhu.%hhu.%hhu : %s\n",
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
    const esp_timer_create_args_t periodic_route_adv_timer_args = {
        .callback = &periodic_route_adv_timer_callback,
        .name = "route_adv"};
    ESP_ERROR_CHECK(esp_timer_create(&periodic_route_adv_timer_args, &periodic_route_adv_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_route_adv_timer, 1000000));
    running = 1;
    ESP_LOGI(TAG, "Routing is started\n");
}

void stop_routing()
{
    running = 0;
    ESP_ERROR_CHECK(esp_timer_stop(periodic_route_adv_timer));
    ESP_ERROR_CHECK(esp_timer_delete(periodic_route_adv_timer));
    remove_gw();
    for (uint8_t i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        routing_tables[i].dest = 0;
        routing_tables[i].next = 0;
        routing_tables[i].out_if = OUT_IFACE_INAVLID;
    }
    unregister_routing_function();
    ESP_LOGI(TAG, "Routing is stopped\n");
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
    ip4_addr_t next_hop = {
        .addr = 0,
    };

    char ifname[3];
    sta_interface = NULL;
    ap_interface = NULL;
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
    if (sta_interface == NULL || ap_interface == NULL)
    {
        ESP_LOGE(TAG, "Interface Down\n");
        return;
    }
    if ((ip4_addr_isbroadcast_u32(iphdr->dest.addr, inp)) || ((iphdr->dest.addr & PP_HTONL(0xf0000000UL)) == PP_HTONL(0xe0000000UL)))
    {
        return;
    }
    struct pbuf *new_p = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_RAM);
    if (new_p == NULL)
    {
        ESP_LOGE(TAG, "Buffer is not allocated\n");
        return;
    }
    if (pbuf_copy(new_p, p) == ERR_ARG)
    {
        ESP_LOGE(TAG, "copy failed\n");
        pbuf_free(new_p);
        new_p = NULL;
        return;
    }
    netif = NULL;
    for (uint8_t i = 0; i < MAX_ROUTING_ENTRIES; i++)
    {
        if (routing_tables[i].dest == iphdr->dest.addr)
        {
            if (routing_tables[i].out_if == OUT_IFACE_AP)
            {
                netif = ap_interface;
                next_hop.addr = routing_tables[i].next;
                break;
            }
            else if (routing_tables[i].out_if == OUT_IFACE_STA)
            {
                netif = sta_interface;
                next_hop.addr = routing_tables[i].next;
                break;
            }
        }
    }
    if (netif == NULL && inp == ap_interface)
    {
        netif = sta_interface;
        next_hop.addr = routing_gw.next;
    }
    if (netif == NULL)
    {
        pbuf_free(new_p);
        new_p = NULL;
        return;
    }
    if (netif->mtu && (new_p->tot_len > netif->mtu))
    {
        if ((IPH_OFFSET(iphdr) & PP_NTOHS(IP_DF)) == 0)
        {
            ip4_frag(new_p, netif, &next_hop);
        }
    }
    else
    {
        netif->output(netif, new_p, &next_hop);
    }
    pbuf_free(new_p);
    new_p = NULL;
}
