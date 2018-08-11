#ifndef DJMESH_FLASH_H
#define DJMESH_FLASH_H
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
    void initFlash();
    void getSSID(char* ssid, uint32_t* length);
    void getPASSWORD(char* passwd, uint32_t* length);
    void getMode(uint8_t* mode);
    void getConnectionRetry(uint8_t* retry);
    void getMagic(uint8_t* magic);

    void setSSID(const char* ssid, uint32_t length);
    void setPASSWORD(const char* passwd, uint32_t length);
    void setMode(const uint8_t mode);
    void setConnectionRetry(const uint8_t retry);
    void setMagic(const uint8_t magic);
#ifdef __cplusplus
}
#endif

#endif
