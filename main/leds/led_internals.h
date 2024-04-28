// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>
#include <driver/gpio.h>


typedef enum {
    OFF = 0,
    ON = 1
} led_string_state_t;


extern const char* LED_TAG;


esp_err_t create_led_strip(gpio_num_t dataPin, gpio_num_t onOffPin, uint32_t ledCount);

esp_err_t set_led_string_on_off(led_string_state_t onOff);

esp_err_t set_led_string_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
esp_err_t refresh_led_string(void);
esp_err_t clear_led_string(void);