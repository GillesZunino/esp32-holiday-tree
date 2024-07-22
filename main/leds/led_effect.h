// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include "leds/led_internals.h"


typedef enum {
    LedAnimationTaskNotificationNone = -1,
    LedAnimationTaskNotificationPause = 0,
    LedAnimationTaskNotificationEffectMin = 1
} led_animation_task_notification_t;


extern const UBaseType_t LedAnimationTaskNotificationIndex;

led_animation_task_notification_t accept_task_notification_with_delay(uint32_t delayMs);

#define WAIT_OR_END_EFECT(delayMs) do { \
        led_animation_task_notification_t _notification = accept_task_notification_with_delay( ( delayMs ) ); \
        if (_notification != LedAnimationTaskNotificationNone) return _notification; \
    } while(0)
