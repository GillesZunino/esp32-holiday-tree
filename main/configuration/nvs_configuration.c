// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------


#include <esp_check.h>
#include <esp_log.h>


// #include <nvs_flash.h>

#include "configuration/nvs_configuration.h"


// NVS operations log tag
static const char* NvsLogTag = "nvs_config";


esp_err_t nvs_get_configuration(const char namespace[NVS_NS_NAME_MAX_SIZE], const char key[NVS_KEY_NAME_MAX_SIZE], void* data, size_t* dataSize) {
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, NvsLogTag, "nvs_get_configuration() - data cannot be NULL");
    ESP_RETURN_ON_FALSE((dataSize != NULL) && (*dataSize > 0), ESP_ERR_INVALID_ARG, NvsLogTag, "nvs_get_configuration() - dataSize must be at least 1");

    nvs_handle_t nvsHandle = 0;
    ESP_RETURN_ON_ERROR(nvs_open(namespace, NVS_READONLY, &nvsHandle), NvsLogTag, "nvs_get_configuration() failed");
    esp_err_t err = nvs_get_blob(nvsHandle, key, data, dataSize);
    nvs_close(nvsHandle);

    return err;
}

esp_err_t nvs_set_configuration(const char namespace[NVS_NS_NAME_MAX_SIZE], const char key[NVS_KEY_NAME_MAX_SIZE], const void* const data, const size_t dataSize) {
    nvs_handle_t nvsHandle = 0;
    
    ESP_RETURN_ON_ERROR(nvs_open(namespace, NVS_READWRITE, &nvsHandle), NvsLogTag, "nvs_set_configuration() failed");
    esp_err_t err = nvs_set_blob(nvsHandle, key, data, dataSize);
    if (err == ESP_OK) {
        err = nvs_commit(nvsHandle);
    }
    nvs_close(nvsHandle);
    
    return err;
}
