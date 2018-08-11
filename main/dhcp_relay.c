#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/raw.h"
#include "lwip/ip.h"
#include "lwip/ip4.h"
#include "lwip/udp.h"
#include "lwip/dhcp.h"
#include "routing.h"
#include "link_manager.h"
#include "dhcp_relay.h"
#include "arp_proxy.h"

static const char *TAG = "DHCP Relay";

static struct netif *sta_interface = NULL;
static struct netif *ap_interface = NULL;

static struct raw_pcb *dhcp_relay_pcb = NULL;
u8_t dhcp_relay_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    char ifname[3];
    struct netif *nif = ip_current_netif();
    struct ip_hdr *const iphdr = (struct ip_hdr *)(p->payload);
    // Find STA netif
    sta_interface = NULL;
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
    ap_interface = NULL;
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
    if(sta_interface == NULL || ap_interface == NULL)
    {
        return 0;
    }
    if (IPH_PROTO(iphdr) != IP_PROTO_UDP)
    {
        return 0;
    }
    struct udp_hdr *const udphdr = (struct udp_hdr *)(p->payload + (IPH_HL(iphdr) * 4));
    if ((udphdr->dest != htons(67) && udphdr->dest != htons(68)))
    {
        return 0;
    }
    struct dhcp_msg *const dhcphdr = (struct dhcp_msg *)((char *)udphdr + sizeof(struct udp_hdr));
    if (dhcphdr->op == 1) // from client to server
    {
        dhcphdr->flags = 0x80;
        if (nif == ap_interface)
        {
            ip4_addr_t dest_addr;
            ip4_addr_copy(dest_addr, iphdr->dest);
            udphdr->chksum = 0;
            //struct pbuf *new_p = pbuf_alloc(PBUF_IP, PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN + p->tot_len, PBUF_RAM);
            struct pbuf *new_p = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_RAM);
            if (!new_p)
            {
                ESP_LOGE(TAG, "Buffer is not allocated\n");
            }
            if (pbuf_copy(new_p, p) == ERR_ARG)
            {
                ESP_LOGE(TAG, "copy failed\n");
            }
            if (sta_interface->output(sta_interface, new_p, &dest_addr) != 0)
            {
                pbuf_free(new_p);
            }
            pbuf_free(p);
            return 1;
        }
    }
    else
    {
        if (nif == sta_interface)
        {
            ip4_addr_t dest_addr;
            ip4_addr_copy(dest_addr, iphdr->dest);
            udphdr->chksum = 0;
            //struct pbuf *new_p = pbuf_alloc(PBUF_IP, PBUF_LINK_ENCAPSULATION_HLEN + PBUF_LINK_HLEN + p->tot_len, PBUF_RAM);
            struct pbuf *new_p = pbuf_alloc(PBUF_IP, p->tot_len, PBUF_RAM);
            if (!new_p)
            {
                ESP_LOGE(TAG, "Buffer is not allocated\n");
            }
            if (pbuf_copy(new_p, p) == ERR_ARG)
            {
                ESP_LOGE(TAG, "copy failed\n");
            }
            uint8_t *p_option = (uint8_t *)dhcphdr->options;
            bool ack = false;
            uint8_t *router_address = NULL;
            while (p_option < (uint8_t *)iphdr + IPH_LEN(iphdr))
            {
                if (p_option[0] == 53 && p_option[1] == 1 && p_option[2] == 5)
                {
                    // Ack
                    ack = true;
                }
                if (p_option[0] == 3)
                {
                    // Router option;
                    router_address = p_option + 2;
                }
                if (ack && router_address)
                {
                    break;
                }
                else
                {
                    p_option = p_option + sizeof(uint8_t) + sizeof(uint8_t) + p_option[1];
                }
            }
            if (ack && router_address)
            {
                ESP_LOGI(TAG, "Recv Ack\n");
                *((uint32_t *)router_address) = ap_interface->ip_addr.u_addr.ip4.addr;
                wifi_sta_list_t sta;
                esp_wifi_ap_get_sta_list(&sta);
                if (sta.num)
                {
                    for (uint8_t i = 0; i < sta.num; i++)
                    {
                        if (sta.sta[i].mac[0] == dhcphdr->chaddr[0] &&
                            sta.sta[i].mac[1] == dhcphdr->chaddr[1] &&
                            sta.sta[i].mac[2] == dhcphdr->chaddr[2] &&
                            sta.sta[i].mac[3] == dhcphdr->chaddr[3] &&
                            sta.sta[i].mac[4] == dhcphdr->chaddr[4] &&
                            sta.sta[i].mac[5] == dhcphdr->chaddr[5])
                        {
                            struct eth_addr ethsrc_addr;
                            struct eth_addr ethdst_addr;
                            struct eth_addr hwsrc_addr; 
                            ip4_addr_t ipsrc_addr;
                            struct eth_addr hwdst_addr;
                            ip4_addr_t ipdst_addr;
                            esp_wifi_get_mac(ESP_IF_WIFI_STA, ethsrc_addr.addr);
                            memset(ethdst_addr.addr, 0xff, 6);
                            esp_wifi_get_mac(ESP_IF_WIFI_STA, hwsrc_addr.addr);
                            memcpy(&ipsrc_addr.addr, &dhcphdr->yiaddr.addr, 4);
                            memset(hwdst_addr.addr, 0x00, 6);
                            memcpy(&ipdst_addr.addr, &dhcphdr->yiaddr.addr, 4);

                            update_routing(dhcphdr->yiaddr.addr, dhcphdr->yiaddr.addr, OUT_IFACE_AP);
                            associate_ip_and_link(sta.sta[i].mac, dhcphdr->yiaddr.addr);
                            if(ESP_OK != etharp_raw(sta_interface, &ethsrc_addr, &ethdst_addr,
                                &hwsrc_addr, &ipsrc_addr,
                                &hwdst_addr, &ipdst_addr,
                                ARP_REQUEST))
                            {
                                ESP_LOGE(TAG, "Failed to send GARP\n");
                            }
                            break;
                        }
                    }
                }
            }
            if (sta_interface->output(ap_interface, new_p, &dest_addr) != 0)
            {
                pbuf_free(new_p);
            }
            pbuf_free(p);
            return 1;
        }
    }
    return 0;
}

static bool init_dhcp_relay()
{
    ip_addr_t bindaddr;
    bindaddr.u_addr.ip4.addr = IPADDR_ANY;
    bindaddr.type = IPADDR_TYPE_V4;
    dhcp_relay_pcb = raw_new(IP_PROTO_UDP);
    if (dhcp_relay_pcb == NULL)
    {
        return false;
    }
    if (ERR_VAL == raw_bind(dhcp_relay_pcb, (ip_addr_t *)&bindaddr))
    {
        return false;
    }
    raw_recv(dhcp_relay_pcb, dhcp_relay_recv, NULL);
    return true;
}

static bool deinit_dhcp_relay()
{
    raw_remove(dhcp_relay_pcb);
    dhcp_relay_pcb = NULL;
    return true;
}

void start_dhcp_relay()
{
    if (init_dhcp_relay() == false)
    {
        ESP_LOGI(TAG, "init_dhcp_relay is failed\n");
        return;
    }
}

void stop_dhcp_relay()
{
    deinit_dhcp_relay();
}
