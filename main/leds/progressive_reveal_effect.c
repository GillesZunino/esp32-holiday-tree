// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/portmacro.h>

#include "leds/led_effect.h"
#include "leds/progressive_reveal_effect.h"


// LED indices on the board
#define CENTER_LED_INDEX 0
#define RIGHT_LED1_INDEX 1
#define RIGHT_LED2_INDEX 2
#define LEFT_LED1_INDEX 3
#define LEFT_LED2_INDEX 4


NotificationType_t progressive_reveal_led_effect() {
    const TickType_t time_per_frame_ms = 1000;
    for (;;) {
        // All Off
        clear_led_string();
        refresh_led_string();
        WAIT_OR_END_EFECT(time_per_frame_ms);

        // Center LED On - white
        set_led_string_pixel(CENTER_LED_INDEX, 255, 255, 255);
        refresh_led_string();
        WAIT_OR_END_EFECT(time_per_frame_ms);

        // Right 1 LED on - Red
        set_led_string_pixel(RIGHT_LED1_INDEX, 255, 0, 0);
        refresh_led_string();
        WAIT_OR_END_EFECT(time_per_frame_ms);

        // Right 2 LED on - Red
        set_led_string_pixel(RIGHT_LED2_INDEX, 255, 0, 0);
        refresh_led_string();
        WAIT_OR_END_EFECT(time_per_frame_ms);

        // Left 1 LED on - Green
        set_led_string_pixel(LEFT_LED1_INDEX, 0, 255, 0);
        refresh_led_string();
        WAIT_OR_END_EFECT(time_per_frame_ms);

        // Left 2 LED on - Green
        set_led_string_pixel(LEFT_LED2_INDEX, 0, 255, 0);
        refresh_led_string();
        WAIT_OR_END_EFECT(time_per_frame_ms);

        // Hold the fully lit step before cycling
        //WAIT_OR_END_EFECT(time_per_frame_ms);
    }
}