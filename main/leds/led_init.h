// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>
#include <soc/gpio_num.h>


esp_err_t configure_led_string(gpio_num_t ledDataPin, gpio_num_t ledOnOffSwitchPin);