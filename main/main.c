// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>


#include "compile_options.h"

#include "gpio/gpio_init.h"

#include "button/button_init.h"

#include "leds_animator/led_init.h"
#include "leds_animator/led_animator.h"
#include "led_effects/progressive_reveal_effect.h"


#include "bt/bt_init.h"



//
// We assign different GPIO pins to various peripherals for development boards versus production boards:
//  * [Real Device]
//      * Momentary button on:
//          -> IO15 which is also JTAG TDO and a strapping pin
//          * -> IO21 (no specific default assignment)
//          -> IO33 which is also ADC1-CH5
//      * WS2812 LED data on IO5 which is also a strapping pin
//  * [Development]
//      * Momentary button on IO23 which has no conflict and allows JTAG use
//      * WS2812 LED data on IO22 which has no conflict
//
#if CONFIG_IDF_TARGET_ESP32
    #if CONFIG_HOLIDAYTREE_HARDWARE_PRODUCTION
        // Momentary button GPIO
        const gpio_num_t ButtonGPIONum = GPIO_NUM_21;

        // Individually addressable LEDs data GPIO
        const gpio_num_t LedDataGPIONum = GPIO_NUM_5;
    #else
        // Momentary button GPIO
        const gpio_num_t ButtonGPIONum = GPIO_NUM_23;

        // Individually addressable LEDs data GPIO
        const gpio_num_t LedDataGPIONum = GPIO_NUM_22;
    #endif
#else
    #error "This project currently only tagets ESP32"
#endif

// Individually addressable LEDs on/off switch GPIO
const gpio_num_t LedSwitchGPIONum = GPIO_NUM_4;

// Main application log tag
static const char* MainTag = "app_main";

// Holiday Sequence LED effect
led_effect_handle_t progressive_reveal_effect_handle;


static void on_momentary_button_pressed(void) {
    ESP_LOGI(MainTag, "on_momentary_button_pressed() Button pressed");
    // Currently no action
}


void app_main(void) {
    // Log compile time options
    ESP_LOGI(MainTag, "%s", CompileTimeOptions);

    // Initialize NVS — It is used to store PHY calibration data
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Configure GPIO pin interrupts
    ESP_ERROR_CHECK(configure_gpio_isr_dispatcher());

    // Configure Bluetooth Classic and start A2DP profile for tree sound player
    ESP_ERROR_CHECK(configure_bluetooth());

    // Configure tree momentary button
    ESP_ERROR_CHECK(configure_momentary_button(ButtonGPIONum, &on_momentary_button_pressed));

    // Configure tree lights
    ESP_ERROR_CHECK(configure_led_string(LedDataGPIONum, LedSwitchGPIONum));
    ESP_ERROR_CHECK(initialize_led_string_effect(progressive_reveal_led_effect, &progressive_reveal_effect_handle));
    ESP_ERROR_CHECK(start_led_string_effect(progressive_reveal_effect_handle, nullptr));

    // Dispatch GPIO events - This function blocks with portMAX_DELAY as timeout and never returns
    ESP_ERROR_CHECK(gpio_events_queue_dispatch());

    //
    // app_main() never returns and does not cleanup allocated objects
    //
}
