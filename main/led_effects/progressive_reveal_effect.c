// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/portmacro.h>

#include "led_effects/progressive_reveal_effect.h"


// LED indices on the board
static const uint32_t CENTER_LED_INDEX = 0;

static const uint32_t RIGHT_LED1_INDEX = 1;
static const uint32_t RIGHT_LED2_INDEX = 2;

static const uint32_t LEFT_LED1_INDEX = 3;
static const uint32_t LEFT_LED2_INDEX = 4;


// Time to wait per frame in milliseconds
static const TickType_t TimePerFrameMs = 1000;


void progressive_reveal_led_effect(led_effect_context_t* context) {
    led_effect_notification_t notification = LedEffectNotificationNone;
    animator_cb_t* animator = &context->animator_cb;

    for (;;) {
        // All Off
        animator->clear_led_string();
        animator->refresh_led_string();
        WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

        // Center LED On - white
        animator->set_led_string_pixel(CENTER_LED_INDEX, 255, 255, 255);
        animator->refresh_led_string();
        WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

        // Right 1 LED on - Red
        animator->set_led_string_pixel(RIGHT_LED1_INDEX, 255, 0, 0);
        animator->refresh_led_string();
        WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

        // Right 2 LED on - Red
        animator->set_led_string_pixel(RIGHT_LED2_INDEX, 255, 0, 0);
        animator->refresh_led_string();
        WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

        // Left 1 LED on - Green
        animator->set_led_string_pixel(LEFT_LED1_INDEX, 0, 255, 0);
        animator->refresh_led_string();
        WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

        // Left 2 LED on - Green
        animator->set_led_string_pixel(LEFT_LED2_INDEX, 0, 255, 0);
        animator->refresh_led_string();
        WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

        // Hold the fully lit step before cycling
        //WAIT_AND_GOTO_ON_SIGNAL(TimePerFrameMs, notification);

notification:
        if (notification == LedEffectNotificationEnd) {
            break;
        }
    }
}