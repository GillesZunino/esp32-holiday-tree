// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include "led_strip.h"

#include "leds/led_internals.h"

// Tag name used on ESP_LOGx macros
const char* LED_TAG = "led_strip";


// LEDs on the Holiday Tree have a resolution of 10MHz
static const uint32_t HolidayTreeLedsResolutionInHz = 10 * 1000 * 1000;


// LEDs strip on / off switch pin
static gpio_num_t s_leds_string_on_off_gpio = GPIO_NUM_NC;

// Strip of individually adressable LEDs
static led_strip_handle_t s_led_strip_handle = NULL;


esp_err_t create_led_strip(gpio_num_t dataPin, gpio_num_t onOffPin, uint32_t ledCount) {
    s_leds_string_on_off_gpio = onOffPin;

    // Configure on/off switch GPIO pin
    gpio_config_t onOff_switch_configuration = {
        .pin_bit_mask = (1ULL << onOffPin),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLDOWN_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&onOff_switch_configuration);
    if (err == ESP_OK) {
        // Configure LED strip for the board
        const led_strip_config_t led_strip_config = {
            .strip_gpio_num = dataPin,
            .max_leds = ledCount,
            .led_pixel_format = LED_PIXEL_FORMAT_GRB,
            .led_model = LED_MODEL_WS2812,
            .flags.invert_out = false
        };
        const led_strip_rmt_config_t rmt_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = HolidayTreeLedsResolutionInHz,
            .mem_block_symbols = 0,
            .flags.with_dma = false
        };

        err = led_strip_new_rmt_device(&led_strip_config, &rmt_config, &s_led_strip_handle);
    }

    return err;
}


esp_err_t set_led_string_on_off(led_string_state_t onOff) {
    return gpio_set_level(s_leds_string_on_off_gpio, onOff == ON ? 1 : 0);
}

esp_err_t set_led_string_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    return led_strip_set_pixel(s_led_strip_handle, index, red, green, blue);
}

esp_err_t refresh_led_string(void) {
    return led_strip_refresh(s_led_strip_handle);
}

esp_err_t clear_led_string(void) {
    return led_strip_clear(s_led_strip_handle);
}