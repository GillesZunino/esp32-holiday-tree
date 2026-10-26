// -----------------------------------------------------------------------------------
// Copyright 2026, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>

#include "led_effects/bluetooth_pairing_effect.h"


typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} rgb_color_t;


// There are five LEDs on the board
//
//     3       2   |---------------
//         0       | ESP32 Antena |
//     4       1   |---------------
//
static const uint32_t LED_COUNT = 5;

// Time to wait per frame and time to wait when LEDs are off in FreeRTOS ticks
static const TickType_t TimePerFrameTicks = pdMS_TO_TICKS(45);
static const TickType_t TimeOffTicks = 1;

// Official Bluetooth 'Blue' color
static const rgb_color_t BluetoothBlue = {0, 130, 252};


static void set_color_intensity(uint16_t percent, rgb_color_t* color);
static void set_string_color(rgb_color_t color, animator_cb_t* animator);


void bluetooth_pairing_led_effect(led_effect_context_t* context) {
    led_effect_notification_t notification = LedEffectNotificationNone;
    animator_cb_t* animator = &context->animator_cb;

    TickType_t wait_time;
    int16_t intensity = 100;
    rgb_color_t color = BluetoothBlue;
    
    for (;;) {
        set_string_color(color, animator);

        if (intensity == 0) {
            intensity = 100;
            color = BluetoothBlue;
            wait_time = TimeOffTicks;
        } else {
            intensity = intensity - 1;
            wait_time = TimePerFrameTicks;
            set_color_intensity(intensity, &color);
        }

        WAIT_AND_GOTO_ON_SIGNAL(wait_time, notification);

notification:
        if (notification == LedEffectNotificationEnd) {
            break;
        }
    }
}

static void set_color_intensity(uint16_t percent, rgb_color_t* color) {
    color->red = (color->red * percent) / 100;
    color->green = (color->green * percent) / 100;
    color->blue = (color->blue * percent) / 100;
}

static void set_string_color(rgb_color_t color, animator_cb_t* animator) {
    for (uint32_t index = 0; index < LED_COUNT; index++) {
        animator->set_led_string_pixel(index, color.red, color.green, color.blue);
    }

    animator->refresh_led_string();
}