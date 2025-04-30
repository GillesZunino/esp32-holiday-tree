// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <esp_check.h>
#include <esp_log.h>

#include "bt/bt_utilities.h"
#include "bt/bt_bd_addr_utils.h"
#include "bt/bt_device_preferences.h"
#include "bt/bt_device_manager.h"
#include "bt/bt_avrc_volume.h"


// Bluetooth device manager log tags
static const char* BtDeviceManagertTag = "bt_device_mgr";


// BDA of the currently connected device, if any
static esp_bd_addr_t s_remote_bda = {0};


static esp_err_t load_device_configuration(const esp_bd_addr_t bda, bt_device_configuration_t* configuration);
static esp_err_t save_device_configuration(const esp_bd_addr_t bda, const bt_device_configuration_t* const configuration);

static void set_device_configuration(const bt_device_configuration_t* const configuration);
static void get_default_device_configuration(bt_device_configuration_t* configuration);

#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
static void log_device_configuration(const char* const message, const esp_bd_addr_t bda, const bt_device_configuration_t* const configuration);
#endif


esp_err_t bt_device_manager_device_connected(const struct avrc_tg_conn_stat_param* const params) {
    // Remember the address of the remote device
    copy_bda(s_remote_bda, params->remote_bda);

    // Apply saved configuration for this device or apply defaults if no previously saved configuration
    bt_device_configuration_t configuration = {0};
    esp_err_t err = load_device_configuration(s_remote_bda, &configuration);
    if (err != ESP_OK) {
        get_default_device_configuration(&configuration);
    }

#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
    log_device_configuration("Configuration loaded from NVS", params->remote_bda, &configuration);
#endif

    set_device_configuration(&configuration);

    return ESP_OK;
}

esp_err_t bt_device_manager_device_disconnected(const struct avrc_tg_conn_stat_param* const params) {
    // Save settings for this device before disconnecting
    bt_device_configuration_t configuration = { .volume = get_volume_avrc() };
    
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
    log_device_configuration("Current configuration", s_remote_bda, &configuration);
#endif

    esp_err_t err = save_device_configuration(s_remote_bda, &configuration);
    if (err != ESP_OK) {
        ESP_LOGE(BtDeviceManagertTag, "[TG] save_device_configuration() failed with %d", err);
    }

    // Clear address of the remote device on disconnect
    clear_bda(s_remote_bda);

    return err;
}

static esp_err_t load_device_configuration(const esp_bd_addr_t bda, bt_device_configuration_t* configuration) {
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (!is_null_bda(bda)) {
        err = get_bt_device_configuration(bda, configuration);
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
        char str[18];
        get_bda_string(bda, str);
        if (err == ESP_OK) {
            ESP_LOGI(BtDeviceManagertTag, "[TG] load_device_configuration() - Loaded saved configuration for device [%s]", str);
        } else {
            ESP_LOGW(BtDeviceManagertTag, "[TG] load_device_configuration() - Could not find configuration for [%s]", str);
        }
    } else {
        ESP_LOGW(BtDeviceManagertTag, "[TG] load_device_configuration() - Could not find configuration - BDA is null");
#endif
    }

    return err;
}

static esp_err_t save_device_configuration(const esp_bd_addr_t bda, const bt_device_configuration_t* const configuration) {
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (!is_null_bda(bda)) {
        err = set_bt_device_configuration(s_remote_bda, configuration);
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
        char str[18];
        get_bda_string(bda, str);
        if (err == ESP_OK) {
            ESP_LOGI(BtDeviceManagertTag, "[TG] save_device_configuration() - Saved configuration for device [%s]", str);
        }
        else {
            ESP_LOGW(BtDeviceManagertTag, "[TG] save_device_configuration() - Failed to save for device [%s] with error %d", str, err);
        }
    } else {
        ESP_LOGW(BtDeviceManagertTag, "[TG] save_device_configuration() - BDA is null, not saving configuration");
#endif
    }

    return err;
}


static void set_device_configuration(const bt_device_configuration_t* const configuration) {
    set_volume_avrc(configuration->volume);
}

static void get_default_device_configuration(bt_device_configuration_t* configuration) {
    configuration->volume = get_default_volume_avrc();
}


#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG

static void log_device_configuration(const char* const message, const esp_bd_addr_t bda, const bt_device_configuration_t* const configuration) {
    char str[18];
    get_bda_string(bda, str);

    ESP_LOGI(BtDeviceManagertTag, "[TG] Device [%s] -> %s", str, message);
    ESP_LOGI(BtDeviceManagertTag, "\t Volume (AVRC): %d", configuration->volume);
}

#endif