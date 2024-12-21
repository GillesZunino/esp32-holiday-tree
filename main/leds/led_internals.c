// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include "led_strip.h"

#include "leds/led_internals.h"


// Tag name used on ESP_LOGx macros
const char* LedStringTag = "led_string";


// LEDs string on / off switch pin
static gpio_num_t s_leds_string_on_off_gpio = GPIO_NUM_NC;

// String of individually adressable LEDs
static led_strip_handle_t s_led_string_handle = NULL;


esp_err_t create_led_string(gpio_num_t dataPin, gpio_num_t onOffPin, uint32_t ledCount) {
    s_leds_string_on_off_gpio = onOffPin;

    // Configure on/off switch GPIO pin
    gpio_config_t onOffSwitchConfiguration = {
        .pin_bit_mask = (1ULL << onOffPin),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLDOWN_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&onOffSwitchConfiguration);
    if (err == ESP_OK) {
        // Configure LED string for the board
        const led_strip_config_t ledStringConfig = {
            .strip_gpio_num = dataPin,
            .max_leds = ledCount,
            .led_model = LED_MODEL_WS2812,
            .color_component_format= {
                .format = {
                    .r_pos = 1, // R is in second position
                    .g_pos = 0, // G is in first position
                    .b_pos = 2, // B is in third position
                    .num_components = 3
                }
            },
            .flags = {
                .invert_out = false
            }
        };

        const led_strip_spi_config_t spiConfig = {
            .clk_src = SPI_CLK_SRC_DEFAULT,
            .spi_bus = SPI2_HOST,
            .flags.with_dma = true
        };
        
        err = led_strip_new_spi_device(&ledStringConfig, &spiConfig, &s_led_string_handle);
    }

    return err;
}


esp_err_t set_led_string_on_off(led_string_state_t onOff) {
    return gpio_set_level(s_leds_string_on_off_gpio, onOff == LedStringOn ? 1 : 0);
}

esp_err_t set_led_string_pixel(uint32_t index, uint32_t red, uint32_t green, uint32_t blue) {
    return led_strip_set_pixel(s_led_string_handle, index, red, green, blue);
}

esp_err_t refresh_led_string(void) {
    return led_strip_refresh(s_led_string_handle);
}

esp_err_t clear_led_string(void) {
    return led_strip_clear(s_led_string_handle);
}