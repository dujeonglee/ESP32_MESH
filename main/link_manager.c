#include <string.h>
#include "esp_log.h"
#include "routing.h"

#include "link_manager.h"

static const char *TAG = "LINKMNG";

struct LinkInfo
{
    uint8_t link_address[6];
    uint32_t ip_address;
    bool is_mesh_link;
    struct LinkInfo *prev;
    struct LinkInfo *next;
};

static struct LinkInfo *head = NULL;
static unsigned char links = 0;

static void add_link(const uint8_t *addr, bool mesh);
static void remove_link(const uint8_t *addr);
static void clear_links();

static void add_link(const uint8_t *addr, bool mesh)
{
    struct LinkInfo *insert;
    if (links == 255)
    {
        return;
    }
    if (links == 0)
    {
        head = malloc(sizeof(struct LinkInfo));
        if (head == NULL)
        {
            return;
        }
        head->prev = NULL;
        head->next = NULL;
    }
    else
    {
        insert = malloc(sizeof(struct LinkInfo));
        if (insert == NULL)
        {
            return;
        }
        insert->prev = NULL;
        insert->next = head;
        head->prev = insert;
        head = insert;
    }
    insert = head;
    insert->link_address[0] = addr[0];
    insert->link_address[1] = addr[1];
    insert->link_address[2] = addr[2];
    insert->link_address[3] = addr[3];
    insert->link_address[4] = addr[4];
    insert->link_address[5] = addr[5];
    insert->ip_address = 0;
    insert->is_mesh_link = mesh;
    links++;
    ESP_LOGI(TAG, "Add link %hhx:%hhx:%hhx:%hhx:%hhx:%hhx [%hhu]\n",
             insert->link_address[0],
             insert->link_address[1],
             insert->link_address[2],
             insert->link_address[3],
             insert->link_address[4],
             insert->link_address[5],
             links);
}

static void remove_link(const uint8_t *addr)
{
    if (links == 1)
    {
        links = 0;
        ESP_LOGI(TAG, "Remove link %hhx:%hhx:%hhx:%hhx:%hhx:%hhx [%hhu]\n",
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
                 links);
        free(head);
        head = NULL;
        return;
    }
    else
    {
        struct LinkInfo *pos = head;
        while (pos)
        {
            if (
                pos->link_address[0] == addr[0] &&
                pos->link_address[1] == addr[1] &&
                pos->link_address[2] == addr[2] &&
                pos->link_address[3] == addr[3] &&
                pos->link_address[4] == addr[4] &&
                pos->link_address[5] == addr[5])
            {
                if (pos->prev)
                {
                    pos->prev->next = pos->next;
                }
                if (pos->next)
                {
                    pos->next->prev = pos->prev;
                }
                if(pos->ip_address != 0)
                {
                    remove_routing(pos->ip_address);
                }
                links--;
                ESP_LOGI(TAG, "Remove link %hhx:%hhx:%hhx:%hhx:%hhx:%hhx [%hhu]\n",
                         addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], 
                         links);
                free(pos);
                return;
            }
            pos = pos->next;
        }
    }
}

static void clear_links()
{
    while(links)
    {
        remove_link(head->link_address);
    }
}

void start_link_manager()
{
    ESP_LOGI(TAG, "Link manager is started\n");
}

void stop_link_manager()
{
    clear_links();
    ESP_LOGI(TAG, "Link manager is stopped\n");
}

void associate_ip_and_link(const uint8_t* mac, uint32_t ip)
{
    struct LinkInfo *pos;
    pos = head;
    while (pos)
    {
        if (
            pos->link_address[0] == mac[0] &&
            pos->link_address[1] == mac[1] &&
            pos->link_address[2] == mac[2] &&
            pos->link_address[3] == mac[3] &&
            pos->link_address[4] == mac[4] &&
            pos->link_address[5] == mac[5])
        {
            pos->ip_address = ip;
            return;
        }
        pos = pos->next;
    }
}

bool is_mesh_link(const unsigned char *mac)
{
    struct LinkInfo *pos;
    pos = head;
    while (pos)
    {
        if (
            pos->link_address[0] == mac[0] &&
            pos->link_address[1] == mac[1] &&
            pos->link_address[2] == mac[2] &&
            pos->link_address[3] == mac[3] &&
            pos->link_address[4] == mac[4] &&
            pos->link_address[5] == mac[5])
        {
            return pos->is_mesh_link;
        }
        pos = pos->next;
    }
    return false;
}

void onStationConnected(system_event_t *event)
{
    system_event_sta_connected_t *info = (system_event_sta_connected_t *)&event->event_info;
    add_link(info->bssid, false);
}

void onStationDisconnected(system_event_t *event)
{
    system_event_sta_disconnected_t *info = (system_event_sta_disconnected_t *)&event->event_info;
    remove_link(info->bssid);
}

void onApStationConnected(system_event_t *event)
{
    system_event_ap_staconnected_t *info = (system_event_ap_staconnected_t *)&event->event_info;
    add_link(info->mac, false);
}

void onApStationDisconnected(system_event_t *event)
{
    system_event_ap_stadisconnected_t *info = (system_event_ap_stadisconnected_t *)&event->event_info;
    remove_link(info->mac);
}

