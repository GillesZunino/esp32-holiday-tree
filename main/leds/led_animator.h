// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>

#include "leds/led_known_effects.h"

esp_err_t start_led_string_effect(led_known_effects_t ledEffect);
esp_err_t stop_led_string_effect();