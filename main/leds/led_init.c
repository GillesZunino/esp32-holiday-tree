// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <driver/gpio.h>


#include "leds/led_internals.h"
#include "leds/led_init.h"


// The Holiday Tree has 5 LEDs
const int HolidayTreeLedsCount = 5;


esp_err_t configure_led_strip(gpio_num_t ledDataPin, gpio_num_t ledOnOffSwitchPin) {
    if ((ledDataPin < GPIO_NUM_0) || (ledDataPin > GPIO_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((ledOnOffSwitchPin < GPIO_NUM_0) || (ledOnOffSwitchPin > GPIO_NUM_MAX)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ledDataPin == ledOnOffSwitchPin) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = create_led_strip(ledDataPin, ledOnOffSwitchPin, HolidayTreeLedsCount);
    if (err == ESP_OK) {
        // Turn strip power on ...
        err = set_led_string_on_off(ON);

        // Clear strip ...
        if (err == ESP_OK) {
            err = clear_led_string();
        }
        
        // Turn strip power back off
        if (err == ESP_OK) {
            err = set_led_string_on_off(OFF);
        }
    }

    return err;
}
