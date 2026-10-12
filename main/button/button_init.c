// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <esp_check.h>

#include "gpio/gpio_init.h"
#include "button/button_init.h"


static const char* ButtonTag = "button";


esp_err_t configure_momentary_button(gpio_num_t buttonGpio, isr_handler_fn_ptr fn) {
    ESP_RETURN_ON_FALSE((buttonGpio >= GPIO_NUM_0) && (buttonGpio < GPIO_NUM_MAX), ESP_ERR_INVALID_ARG, ButtonTag, "Invalid button GPIO '%d'", (int) buttonGpio);
    ESP_RETURN_ON_FALSE(fn != NULL, ESP_ERR_INVALID_ARG, ButtonTag, "NULL button handler 'fn'");

    gpio_config_t buttonPinConfiguration = {
        .pin_bit_mask = (1ULL << buttonGpio),
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    
    esp_err_t err = gpio_config(&buttonPinConfiguration);
    if (err == ESP_OK) {
        return ht_gpio_isr_handler_add(buttonGpio, fn);
    }

    return err;
}