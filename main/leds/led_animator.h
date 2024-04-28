// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>

#include "leds/led_known_effects.h"

esp_err_t start_led_strip_effect(LEDEffect_t led_effect);
esp_err_t stop_led_strip_effect(void);