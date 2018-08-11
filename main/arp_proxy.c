#include <stdio.h>
#include <string.h>
// #include "netif/ethernet.h"
// #include "netif/raw_ethernet.h"
// #include "netif/etharp.h"
// #include "esp_wifi.h"
#include "esp_log.h"

#include "routing.h"
#include "arp_proxy.h"

static char* TAG = "PROXY-ARP";

static struct raw_eth_pcb *pcb = NULL;
static struct eth_addr sta_mac;
static struct eth_addr ap_mac;
static struct netif *sta_interface = NULL;
static struct netif *ap_interface = NULL;

err_t etharp_raw(struct netif *netif, const struct eth_addr *ethsrc_addr,
                 const struct eth_addr *ethdst_addr,
                 const struct eth_addr *hwsrc_addr, const ip4_addr_t *ipsrc_addr,
                 const struct eth_addr *hwdst_addr, const ip4_addr_t *ipdst_addr,
                 const u16_t opcode)
{
    struct pbuf *p;
    err_t result = ERR_OK;
    struct eth_hdr *ethhdr;
    struct etharp_hdr *hdr;

    /* allocate a pbuf for the outgoing ARP request packet */
    p = pbuf_alloc(PBUF_RAW_TX, SIZEOF_ETHARP_PACKET_TX, PBUF_RAM);
    /* could allocate a pbuf for an ARP request? */
    if (p == NULL)
    {
        return ERR_MEM;
    }

    ethhdr = (struct eth_hdr *)p->payload;
    hdr = (struct etharp_hdr *)((u8_t *)ethhdr + SIZEOF_ETH_HDR);
    hdr->opcode = htons(opcode);

    /* Write the ARP MAC-Addresses */
    ETHADDR16_COPY(&hdr->shwaddr, hwsrc_addr);
    ETHADDR16_COPY(&hdr->dhwaddr, hwdst_addr);
    /* Copy struct ip4_addr2 to aligned ip4_addr, to support compilers without
   * structure packing. */
    IPADDR2_COPY(&hdr->sipaddr, ipsrc_addr);
    IPADDR2_COPY(&hdr->dipaddr, ipdst_addr);

    hdr->hwtype = PP_HTONS(1);
    hdr->proto = PP_HTONS(ETHTYPE_IP);
    /* set hwlen and protolen */
    hdr->hwlen = ETH_HWADDR_LEN;
    hdr->protolen = sizeof(ip4_addr_t);

    ethhdr->type = PP_HTONS(ETHTYPE_ARP);

    ETHADDR16_COPY(&ethhdr->dest, ethdst_addr);
    ETHADDR16_COPY(&ethhdr->src, ethsrc_addr);

    /* send ARP query */
    result = netif->linkoutput(netif, p);
    /* free ARP query packet */
    pbuf_free(p);
    p = NULL;
    /* could not allocate pbuf for ARP request */
    return result;
}

void arp_recv(void *arg, const struct raw_eth_pcb *pcb, struct pbuf *p, struct netif *nif)
{
    uint32_t target = 0;
    uint32_t sender = 0;
    struct etharp_hdr *arphdr = (struct etharp_hdr *)(p->payload + sizeof(struct eth_hdr));
    char ifname[3];
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
        return;
    }
    if (arphdr->hwtype != PP_HTONS(1))
    {
        return;
    }
    if (arphdr->proto != PP_HTONS(ETHTYPE_IP))
    {
        return;
    }
    if (arphdr->hwlen != ETH_HWADDR_LEN)
    {
        return;
    }
    if (arphdr->protolen != sizeof(ip4_addr_t))
    {
        return;
    }
    if (arphdr->opcode != PP_HTONS(ARP_REQUEST))
    {
        return;
    }
    memcpy(&sender, arphdr->sipaddr.addrw, sizeof(sender));
    memcpy(&target, &arphdr->dipaddr.addrw, sizeof(target));
    if (sender == 0)
    {
        return;
    }
    if ((nif == ap_interface && mesh_client(sender) /*&& arphdr->shwaddr is not in the mesh list*/))
    {
        etharp_raw(nif,
                   (struct eth_addr *)&sta_mac,
                   &arphdr->shwaddr,
                   (struct eth_addr *)&sta_mac,
                   (ip4_addr_t *)arphdr->dipaddr.addrw,
                   &arphdr->shwaddr,
                   (ip4_addr_t *)arphdr->sipaddr.addrw,
                   ARP_REPLY);
    }
    else if ((nif == sta_interface && mesh_client(target) /*&& arphdr->shwaddr is not in the mesh list*/))
    {
        etharp_raw(nif,
                   (struct eth_addr *)&sta_mac,
                   &arphdr->shwaddr,
                   (struct eth_addr *)&sta_mac,
                   (ip4_addr_t *)arphdr->dipaddr.addrw,
                   &arphdr->shwaddr,
                   (ip4_addr_t *)arphdr->sipaddr.addrw,
                   ARP_REPLY);
    }
}

void start_arp_proxy()
{
    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac.addr);
    esp_wifi_get_mac(ESP_IF_WIFI_AP, ap_mac.addr);
    pcb = raw_eth_new(ETHTYPE_ARP);
    if (pcb)
    {
        raw_eth_recv(pcb, arp_recv, NULL);
    }
    ESP_LOGI(TAG, "Proxy ARP is started\n");
}

void stop_arp_proxy()
{
    raw_eth_remove(pcb);
    pcb = NULL;
    ESP_LOGI(TAG, "Proxy ARP is stopped\n");
}
