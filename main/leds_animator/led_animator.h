// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>

#include "leds_animator/led_effect.h"


// Opaque handle to a LED effect instance
struct led_effect;
typedef struct led_effect* led_effect_handle_t;


esp_err_t initialize_led_string_effect(led_effect_callback_t effect_fun, led_effect_handle_t* pEffect_handle);
esp_err_t free_led_string_effect(led_effect_handle_t handle);

esp_err_t start_led_string_effect(led_effect_handle_t handle, void* context);
esp_err_t stop_current_led_string_effect(void);