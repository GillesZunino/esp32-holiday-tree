// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------


#pragma once

#include <esp_bt_defs.h>


typedef struct bt_device_preferences {
    uint8_t blob_size;
    uint8_t volume;
} __attribute__ ((__packed__)) bt_device_preferences_t;


esp_err_t get_bt_device_preferences(const esp_bd_addr_t bda, bt_device_preferences_t* preferences);
esp_err_t set_bt_device_preferences(const esp_bd_addr_t bda, const bt_device_preferences_t* const preferences);
