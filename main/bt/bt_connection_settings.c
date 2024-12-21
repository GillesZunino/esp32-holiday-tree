// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <esp_check.h>
#include <esp_log.h>

#include <nvs.h>
#include <nvs_flash.h>

#include "bt/bt_connection_settings.h"


// NVS operations log tag
static const char* NvsLogTag = "nvs_ops";

// NVS namespace for Bluetooth devices preferences
static const char* const BTDevicesPreferences = "bt_devices";

// Length of the NVS key for a Bluetooth device preferences, including the null-terminator
static const uint8_t PreferenceKeyLength = 12 + 1;


static void bda_to_nvs_key(const esp_bd_addr_t bda, char key[PreferenceKeyLength]);


esp_err_t get_bt_device_preferences(const esp_bd_addr_t bda, bt_device_preferences_t* preferences) {
    nvs_handle_t btDevicesNvsHandle = 0;
    ESP_RETURN_ON_ERROR(nvs_open(BTDevicesPreferences, NVS_READONLY, &btDevicesNvsHandle), NvsLogTag, "get_bt_device_preferences() failed");

    size_t preferencesSize = sizeof(bt_device_preferences_t);
    uint8_t* preferencesBuffer = malloc(preferencesSize);

    char preferencesKey[PreferenceKeyLength];
    bda_to_nvs_key(bda, preferencesKey);

    esp_err_t err = ESP_OK;
    if (preferencesBuffer != NULL) {
        err = nvs_get_blob(btDevicesNvsHandle, preferencesKey, preferencesBuffer, &preferencesSize);
        if (err == ESP_OK) {
            memcpy(preferences, preferencesBuffer, preferencesSize);
        }
        free(preferencesBuffer);
    } else {
        err = ESP_ERR_NO_MEM;
    }

    nvs_close(btDevicesNvsHandle);

    return err;
}

esp_err_t set_bt_device_preferences(const esp_bd_addr_t bda, const bt_device_preferences_t* const preferences) {
    nvs_handle_t btDevicesNvsHandle = 0;
    ESP_RETURN_ON_ERROR(nvs_open(BTDevicesPreferences, NVS_READWRITE, &btDevicesNvsHandle), NvsLogTag, "set_bt_device_preferences() failed");

    size_t preferencesSize = sizeof(bt_device_preferences_t);

    char preferencesKey[PreferenceKeyLength];
    bda_to_nvs_key(bda, preferencesKey);

    esp_err_t err = nvs_set_blob(btDevicesNvsHandle, preferencesKey, preferences, preferencesSize);
    if (err == ESP_OK) {
        err = nvs_commit(btDevicesNvsHandle);
    }

    nvs_close(btDevicesNvsHandle);
    
    return err;
}

static void bda_to_nvs_key(const esp_bd_addr_t bda, char key[PreferenceKeyLength]) {
    sprintf(key, "%02x%02x%02x%02x%02x%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}