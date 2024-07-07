// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <freertos/FreeRTOS.h>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_bt.h>

#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_gap_bt_api.h>


#include "bt/bt_init.h"
#include "bt/bt_work_dispatcher.h"
#include "bt/bt_gap.h"
#include "bt/bt_avrc.h"
#include "bt/bt_a2d.h"


// Bluetooth application log tag
static const char* BT_APP_TAG = "bt_app";


static esp_err_t start_bluetooth();
static esp_err_t set_device_name(const char* deviceName);
static esp_err_t setup_bluetooth_profiles();
static esp_err_t configure_scan_mode();


esp_err_t configure_bluetooth() {
    // Start Bluetooth (BlueDroid with Classic BT only)
    ESP_RETURN_ON_ERROR(start_bluetooth(), BT_APP_TAG, "start_bluetooth() failed");

    // Start the task and queue used to execute work posted by the Bluetooth stack
    ESP_RETURN_ON_ERROR(start_bluetooth_dispatcher_task(), BT_APP_TAG, "start_bluetooth_dispatcher_task() failed");

    // Set Bluetooth device name
    ESP_RETURN_ON_ERROR(set_device_name(CONFIG_HOLIDAYTREE_BR_EDR_DEVICE_NAME_STR), BT_APP_TAG, "set_device_name(CONFIG_HOLIDAYTREE_BR_EDR_DEVICE_NAME_STR) failed");

    // Add and configure profiles we will need
    ESP_RETURN_ON_ERROR(setup_bluetooth_profiles(), BT_APP_TAG, "setup_bluetooth_profiles() failed");

    // Set discoverable and connectable - Wait for connection
    ESP_RETURN_ON_ERROR(configure_scan_mode(), BT_APP_TAG, "configure_scan_mode() failed");
    return ESP_OK;
}

static esp_err_t start_bluetooth() {
    // We only use Bluetooth Classic - Release memory occupied by Bluetooth Low Energy 
    ESP_RETURN_ON_ERROR(esp_bt_controller_mem_release(ESP_BT_MODE_BLE), BT_APP_TAG, "esp_bt_controller_mem_release(ESP_BT_MODE_BLE) failed");

    // Initialize Blueetooth controller
    esp_bt_controller_config_t btControllerConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&btControllerConfig), BT_APP_TAG, "esp_bt_controller_init() failed");

    // Enable Bluetooth controller
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), BT_APP_TAG, "esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) failed");

    // Configure BlueDroid
    esp_bluedroid_config_t bluedroidConfig = {
#if (CONFIG_HOLIDAYTREE_BR_EDR_LEGACY_PAIRING_REQUIRE_STATIC_PIN == true)
        // Turn off SSP and use Legacy Pairing
        .ssp_en = false
#else
        // Leave Simple Secure Pairing on - The device has no way to show the pin to users anyway
        .ssp_en = true
#endif
    };
    ESP_RETURN_ON_ERROR(esp_bluedroid_init_with_cfg(&bluedroidConfig), BT_APP_TAG, "esp_bluedroid_init_with_cfg() failed");

    // Enable BlueDroid
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), BT_APP_TAG, "esp_bluedroid_enable() failed");

#if (CONFIG_HOLIDAYTREE_BR_EDR_LEGACY_PAIRING_REQUIRE_STATIC_PIN == true)
    // Enable fixed pin during legacy pairing, if requested
    esp_bt_pin_code_t pin_code;

    uint8_t pinLength = strlen(CONFIG_HOLIDAYTREE_BR_EDR_STATIC_PIN_STR);
    pinLength = pinLength <= ESP_BT_PIN_CODE_LEN ? pinLength : ESP_BT_PIN_CODE_LEN;
    memcpy(pin_code, CONFIG_HOLIDAYTREE_BR_EDR_STATIC_PIN_STR, pinLength);

    ESP_RETURN_ON_ERROR(esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, pinLength, pin_code), BT_APP_TAG, "esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED) failed");
#endif

    return ESP_OK;
}

static esp_err_t set_device_name(const char* deviceName) {
    ESP_RETURN_ON_ERROR(esp_bt_dev_set_device_name(CONFIG_HOLIDAYTREE_BR_EDR_DEVICE_NAME_STR), BT_APP_TAG, "esp_bt_dev_set_device_name(CONFIG_HOLIDAYTREE_BR_EDR_DEVICE_NAME_STR) failed");
    return ESP_OK;
}

static esp_err_t setup_bluetooth_profiles() {
    // Configure GAP
    ESP_RETURN_ON_ERROR(setup_gap_profile(), BT_APP_TAG, "setup_gap_profile() failed");

    // Configure AVRC
    ESP_RETURN_ON_ERROR(setup_avrc_profile(), BT_APP_TAG, "setup_avrc_profile() failed");
    
    // Configure A2D
    ESP_RETURN_ON_ERROR(setup_a2d_profile(), BT_APP_TAG, "setup_a2d_profile()");
    return ESP_OK;
}

static esp_err_t configure_scan_mode() {
    // Make broadly discoverable and connectable
    ESP_RETURN_ON_ERROR(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE), BT_APP_TAG, "esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE) failed");
    return ESP_OK;
}