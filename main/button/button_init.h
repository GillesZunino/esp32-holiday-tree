// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>
#include <driver/gpio.h>


esp_err_t configure_momentary_button(gpio_num_t buttonGpio, isr_handler_fn_ptr fn);