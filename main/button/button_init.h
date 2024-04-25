// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <driver/gpio.h>


esp_err_t configure_button(gpio_num_t buttonGpio, isr_handler_fn_ptr fn);