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

#include "bcmc_forward.h"

static const char *TAG = "BCMC FWD";

static struct netif *sta_interface = NULL;
static struct netif *ap_interface = NULL;

static struct raw_pcb *bcmc_fwd_pcb = NULL;
u8_t bcmc_fwd_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    char ifname[3];
    struct netif *nif = ip_current_netif();
    struct netif *out_nif = NULL;
    ip4_addr_t dest_addr = {
        .addr = 0,
    };
    struct ip_hdr *const iphdr = (struct ip_hdr *)(p->payload);
    if ((p->flags & (PBUF_FLAG_LLBCAST | PBUF_FLAG_LLMCAST)) == 0)
    {
        return 0;
    }
    if (IPH_PROTO(iphdr) != IP_PROTO_UDP)
    {
        return 0;
    }
    struct udp_hdr *const udphdr = (struct udp_hdr *)(p->payload + (IPH_HL(iphdr) * 4));
    if ((udphdr->dest == htons(67) || udphdr->dest == htons(68)))
    {
        return 0;
    }
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
    out_nif = (nif == ap_interface ? sta_interface : ap_interface);
    udphdr->chksum = 0;
    struct pbuf *new_p = pbuf_alloc(PBUF_LINK, p->tot_len, PBUF_RAM);
    if (!new_p)
    {
        ESP_LOGE(TAG, "Buffer is not allocated\n");
        return 0;
    }
    if (pbuf_copy(new_p, p) == ERR_ARG)
    {
        ESP_LOGE(TAG, "copy failed\n");
        pbuf_free(new_p);
        new_p = NULL;
        return 0;
    }
    dest_addr.addr = iphdr->dest.addr;
    out_nif->output(out_nif, new_p, &(dest_addr));
    pbuf_free(new_p);
    new_p = NULL;
    return 0;
}

static bool init_bcmc_forward()
{
    ip_addr_t bindaddr;
    bindaddr.u_addr.ip4.addr = IPADDR_ANY;
    bindaddr.type = IPADDR_TYPE_V4;
    bcmc_fwd_pcb = raw_new(IP_PROTO_UDP);
    if (bcmc_fwd_pcb == NULL)
    {
        return false;
    }
    if (ERR_VAL == raw_bind(bcmc_fwd_pcb, (ip_addr_t *)&bindaddr))
    {
        return false;
    }
    raw_recv(bcmc_fwd_pcb, bcmc_fwd_recv, NULL);
    return true;
}

static bool deinit_bcmc_forward()
{
    raw_remove(bcmc_fwd_pcb);
    bcmc_fwd_pcb = NULL;
    return true;
}

void start_bcmc_forward()
{
    if (init_bcmc_forward() == false)
    {
        ESP_LOGI(TAG, "init_dhcp_relay is failed\n");
        return;
    }
}

void stop_bcmc_forward()
{
    deinit_bcmc_forward();
}
