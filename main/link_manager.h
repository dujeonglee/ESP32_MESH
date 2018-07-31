#ifndef DJMESH_LINK_MANAGER_H
#define DJMESH_LINK_MANAGER_H
#include <stdint.h>
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C"
{
#endif
    void start_link_manager();
    void stop_link_manager();
    void associate_ip_and_link(const uint8_t* mac, uint32_t ip);
    bool is_mesh_link(const uint8_t* mac);

    void onStationConnected(system_event_t *event);
    void onStationDisconnected(system_event_t *event);
    void onApStationConnected(system_event_t *event);
    void onApStationDisconnected(system_event_t *event);
#ifdef __cplusplus
}
#endif

#endif
