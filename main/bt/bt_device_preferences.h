// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------


#pragma once

#include <esp_bt_defs.h>

#include "bt/bt_device_configuration.h"


esp_err_t get_bt_device_configuration(const esp_bd_addr_t bda, bt_device_configuration_t* const configuration);
esp_err_t set_bt_device_configuration(const esp_bd_addr_t bda, const bt_device_configuration_t* const configuration);