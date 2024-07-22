// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include "gpio/gpio_init.h"
#include "button/button_init.h"


esp_err_t configure_momentary_button(gpio_num_t buttonGpio, isr_handler_fn_ptr fn) {
    if ((buttonGpio < GPIO_NUM_0) || (buttonGpio > GPIO_NUM_MAX) || (fn == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t buttonPinConfiguration = {
        .pin_bit_mask = (1ULL << buttonGpio),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLDOWN_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    esp_err_t err = gpio_config(&buttonPinConfiguration);
    if (err == ESP_OK) {
        return ht_gpio_isr_handler_add(buttonGpio, fn);
    }

    return err;
}