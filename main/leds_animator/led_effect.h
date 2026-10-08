// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>


// Notification the animator can send to an LED effect
typedef enum {
    LedEffectNotificationNone = -1,
    LedEffectNotificationEnd = 0,
} led_effect_notification_t;


// Animator callbacks provided to the LED effect
typedef struct animator_cb {
    esp_err_t (*set_led_string_on_off)(bool onOff);
    esp_err_t (*set_led_string_pixel)(uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
    esp_err_t (*refresh_led_string)(void);
    esp_err_t (*clear_led_string)(void);
    led_effect_notification_t (*delay_with_parent_signals)(TickType_t xTicksToWait);
} animator_cb_t;


// Context passed to the LED effect by the animator
typedef struct led_effect_context {
    animator_cb_t animator_cb;
    void* context;
} led_effect_context_t;


// LED effect callback type
typedef void (*led_effect_callback_t)(led_effect_context_t* context);


// Macro an LED effect can use to wait for a certain delay while listening to parent signals
#define WAIT_AND_GOTO_ON_SIGNAL(delay_ms, goto_tag) do { \
        led_effect_notification_t _notification = animator->delay_with_parent_signals((delay_ms)); \
        if (unlikely(_notification != LedEffectNotificationNone)) { \
            notification = _notification; \
            goto goto_tag; \
        } \
    } while(0)
