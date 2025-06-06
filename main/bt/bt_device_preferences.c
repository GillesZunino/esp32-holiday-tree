// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------


#include <string.h>

#include <esp_check.h>
#include <esp_log.h>

#include "configuration/nvs_configuration.h"

#include "bt/bt_device_preferences.h"


// Wrap the effective configuration with the version number to facilitate build to build upgrades
typedef struct bt_device_configuration_blob {
    uint16_t version;
    bt_device_configuration_t configuration;
} __attribute__ ((__packed__)) bt_device_configuration_blob_t;


// NVS namespace for Bluetooth devices preferences
static const char BTDevicesPreferencesNamespace[NVS_NS_NAME_MAX_SIZE] = "bt_devices";

// Current configuration version
static const uint16_t CurrentConfigurationVersion = 1;


static void bda_to_nvs_key(const esp_bd_addr_t bda, char key[NVS_KEY_NAME_MAX_SIZE]);


esp_err_t get_bt_device_configuration(const esp_bd_addr_t bda, bt_device_configuration_t* const configuration) {
    char configurationKey[NVS_KEY_NAME_MAX_SIZE];
    bda_to_nvs_key(bda, configurationKey);

    bt_device_configuration_blob_t configurationBuffer = {0};
    size_t configurationSize = sizeof(configurationBuffer);

    esp_err_t err = nvs_get_configuration(BTDevicesPreferencesNamespace, configurationKey, &configurationBuffer, &configurationSize);
    if (err == ESP_OK) {
        // CONSIDER: Upgrade / convert configuration if needed
        memcpy(configuration, &configurationBuffer.configuration, sizeof(bt_device_configuration_t));
    }

    return err;
}

esp_err_t set_bt_device_configuration(const esp_bd_addr_t bda, const bt_device_configuration_t* const configuration) {
    char configurationKey[NVS_KEY_NAME_MAX_SIZE];
    bda_to_nvs_key(bda, configurationKey);

    bt_device_configuration_blob_t configurationBuffer = { .version = CurrentConfigurationVersion };
    memcpy(&configurationBuffer.configuration, configuration, sizeof(bt_device_configuration_t));

    return nvs_set_configuration(BTDevicesPreferencesNamespace, configurationKey, &configurationBuffer, sizeof(bt_device_configuration_blob_t));
}

static void bda_to_nvs_key(const esp_bd_addr_t bda, char key[NVS_KEY_NAME_MAX_SIZE]) {
    sprintf(key, "%02x%02x%02x%02x%02x%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
}