// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include "leds/led_internals.h"


typedef enum {
    NotificationNone = -1,
    NotificationPause = 0,
    NotificationEffectMin = 1
} NotificationType_t;


extern const UBaseType_t ledAnimationTaskNotificationIndex;

NotificationType_t accept_task_notification_with_delay(uint32_t delayMs);

#define WAIT_OR_END_EFECT(delayMs) do { \
        uint32_t _notification = accept_task_notification_with_delay( ( delayMs ) ); \
        if (_notification < NotificationEffectMin) return _notification; \
    } while(0)
