/* Simple WiFi Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_heap_trace.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "dhcp_relay.h"
#include "bcmc_forward.h"
#include "arp_proxy.h"
#include "routing.h"
#include "link_manager.h"
#include "flash.h"

#define AP_SSID "happyjoy2.4_EXT"
#define AP_PW "18487140"

char SSID[32];
uint32_t SSID_LENGTH;
char PASSWD[64];
uint32_t PASSWD_LENGTH;
uint8_t MODE;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "Mesh";

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id)
    {
    case SYSTEM_EVENT_WIFI_READY: /**< ESP32 WiFi ready */
    {
        ESP_LOGE(TAG, "WIFI is ready\n");
    }
    break;
    case SYSTEM_EVENT_SCAN_DONE: /**< ESP32 finish scanning AP */
    {
        uint16_t aps = 0;
        wifi_ap_record_t *apList = NULL;

        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&aps));
        apList = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * aps);
        if (apList == NULL)
        {
            ESP_LOGE(TAG, "fail: malloc\n");
            esp_restart();
        }
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&aps, apList));
        for (uint16_t i = 0; i < aps; i++)
        {
            if (strcmp((char *)apList[i].ssid, "happyjoy2.4_EXT") == 0)
            {
                wifi_config_t wifi_config = {
                    .sta = 
                    {
                        .ssid = "happyjoy2.4_EXT",
                        .password = "18487140",
                    },
                };
                /*
                memcpy(wifi_config.sta.ssid, SSID, SSID_LENGTH);
                memcpy(wifi_config.sta.password, PASSWD, PASSWD_LENGTH);
                */

                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
                ESP_ERROR_CHECK(esp_wifi_connect());
                free(apList);
                return ESP_OK;
            }
        }
        ESP_LOGE(TAG, "Cannot find the target AP\n");
        free(apList);
        esp_restart();
    }
    break;
    case SYSTEM_EVENT_STA_START: /**< ESP32 station start */
    {
        wifi_scan_config_t scan_config;
        memset(&scan_config, 0, sizeof(scan_config));
        scan_config.show_hidden = true;
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
        break;
    }
    break;
    case SYSTEM_EVENT_STA_STOP: /**< ESP32 station stop */
        break;
    case SYSTEM_EVENT_STA_CONNECTED: /**< ESP32 station connected to AP */
        onStationConnected(event);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED: /**< ESP32 station disconnected from AP */
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        onStationDisconnected(event);
        break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected by ESP32 station changed */
        break;
    case SYSTEM_EVENT_STA_GOT_IP: /**< ESP32 station got IP from connected AP */
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        ESP_LOGI(TAG, "Enable softAP\n");
        wifi_config_t wifi_config = {
            .ap = {
                .ssid = "Mesh",
                .ssid_len = strlen("Mesh"),
                .password = "12345678",
                .max_connection = 4,
                .authmode = WIFI_AUTH_WPA_WPA2_PSK
            },
        };

        tcpip_adapter_dhcps_stop(ESP_IF_WIFI_AP);
        tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &(event->event_info.got_ip.ip_info));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
        start_dhcp_relay();
        start_bcmc_forward();
        start_arp_proxy();
        start_routing(event->event_info.got_ip.ip_info.gw.addr, OUT_IFACE_STA);
        break;
    case SYSTEM_EVENT_STA_LOST_IP: /**< ESP32 station lost IP and the IP is reset to 0 */
        break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS: /**< ESP32 station wps succeeds in enrollee mode */
        break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED: /**< ESP32 station wps fails in enrollee mode */
        break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT: /**< ESP32 station wps timeout in enrollee mode */
        break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN: /**< ESP32 station wps pin code in enrollee mode */
        break;
    case SYSTEM_EVENT_AP_START: /**< ESP32 soft-AP start */
        break;
    case SYSTEM_EVENT_AP_STOP: /**< ESP32 soft-AP stop */
        break;
    case SYSTEM_EVENT_AP_STACONNECTED: /**< a station connected to ESP32 soft-AP */
        onApStationConnected(event);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED: /**< a station disconnected from ESP32 soft-AP */
        onApStationDisconnected(event);
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED: /**< ESP32 soft-AP assign an IP to a connected station */
        break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED: /**< Receive probe request packet in soft-AP interface */
        break;
    case SYSTEM_EVENT_GOT_IP6: /**< ESP32 station or ap or ethernet interface v6IP addr is preferred */
        break;
    case SYSTEM_EVENT_ETH_START: /**< ESP32 ethernet start */
        break;
    case SYSTEM_EVENT_ETH_STOP: /**< ESP32 ethernet stop */
        break;
    case SYSTEM_EVENT_ETH_CONNECTED: /**< ESP32 ethernet phy link up */
        break;
    case SYSTEM_EVENT_ETH_DISCONNECTED: /**< ESP32 ethernet phy link down */
        break;
    case SYSTEM_EVENT_ETH_GOT_IP: /**< ESP32 ethernet got IP from connected AP */
        break;
    default:
        break;
    }
    return ESP_OK;
}

/*

*/

void wifi_init_mesh()
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    start_link_manager();
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main()
{
    //Initialize NVS
    ESP_ERROR_CHECK( heap_trace_init_standalone(trace_record, NUM_RECORDS) );
    initFlash();
    SSID_LENGTH = sizeof(SSID);
    getSSID(SSID, &SSID_LENGTH);
    PASSWD_LENGTH = sizeof(PASSWD);
    getPASSWORD(PASSWD, &PASSWD_LENGTH);
    getMode(&MODE);
    ESP_LOGI(TAG, "Load Mesh AP Info SSID: %s PASSWD: %s Mode : %u\n", SSID, PASSWD, MODE);
    wifi_init_mesh();
}
