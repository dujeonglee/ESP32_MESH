#include "nvs_flash.h"
#include "nvs.h"

#include "flash.h"

void initFlash()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void getSSID(char* ssid, uint32_t* length)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle handle;
    err = nvs_open("APINFO", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        (*length) = 0;
        return;
    }
    err = nvs_get_str(handle, "SSID", ssid, length);
    if(err != ESP_OK)
    {
        (*length) = 0;
    }
    nvs_close(handle);
}

void getPASSWORD(char* passwd, uint32_t* length)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle handle;
    err = nvs_open("APINFO", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        (*length) = 0;
        return;
    }
    err = nvs_get_str(handle, "PASSWORD", passwd, length);
    if(err != ESP_OK)
    {
        (*length) = 0;
    }
    nvs_close(handle);
}

void getMode(uint8_t* mode)
{
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle handle;
    err = nvs_open("APINFO", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        (*mode) = 0xff;
        return;
    }
    err = nvs_get_u8(handle, "MODE", mode);
    if(err != ESP_OK)
    {
        (*mode) = 0xff;
    }
    nvs_close(handle);
}

void setSSID(const char* ssid, uint32_t length)
{
    esp_err_t err;
    
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle handle;
    err = nvs_open("APINFO", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;
    } 
    // Write
    err = nvs_set_str(handle, "SSID", ssid);
    if(err != ESP_OK)
    {
        nvs_close(handle);
        return;
    }
    err = nvs_commit(handle);
    if(err != ESP_OK)
    {
        nvs_close(handle);
        return;
    }
    nvs_close(handle);
}

void setPASSWORD(const char* passwd, uint32_t length)
{
    esp_err_t err;
    
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle handle;
    err = nvs_open("APINFO", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;
    } 
    // Write
    err = nvs_set_str(handle, "PASSWORD", passwd);
    if(err != ESP_OK)
    {
        nvs_close(handle);
        return;
    }
    err = nvs_commit(handle);
    if(err != ESP_OK)
    {
        nvs_close(handle);
        return;
    }
    nvs_close(handle);
}

void setMode(const uint8_t mode)
{
    esp_err_t err;
    
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Open
    nvs_handle handle;
    err = nvs_open("APINFO", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return;
    } 
    // Write
    err = nvs_set_u8(handle, "MODE", mode);
    if(err != ESP_OK)
    {
        nvs_close(handle);
        return;
    }
    err = nvs_commit(handle);
    if(err != ESP_OK)
    {
        nvs_close(handle);
        return;
    }
    nvs_close(handle);
}
