// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include "leds/led_effect.h"
#include "leds/led_known_effects.h"

#include "leds/progressive_reveal_effect.h"


static void animate_led_task(void* arg);
static esp_err_t turn_led_string_on_off(led_string_state_t on);


// FreeRTOS task notification index for LED animation task notifications
const UBaseType_t LedAnimationTaskNotificationIndex = 0;

// LEDs animation task
static TaskHandle_t s_animate_led_task_handle = NULL;


static const char* get_led_task_notification_name(led_animation_task_notification_t notification);
static const char* get_led_effect_name(led_known_effects_t ledEffect);


esp_err_t start_led_string_effect(led_known_effects_t ledEffect) {
    if (s_animate_led_task_handle == NULL) {
        BaseType_t outcome = xTaskCreate(animate_led_task, "ht-leds-anim", 3072, NULL, 10, &s_animate_led_task_handle);
        if (outcome != pdPASS) {
            s_animate_led_task_handle = NULL;
            return ESP_FAIL;
        }
    }

    BaseType_t outcome = xTaskNotifyIndexed(s_animate_led_task_handle, LedAnimationTaskNotificationIndex, ledEffect, eSetValueWithOverwrite);
    return outcome == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t stop_led_string_effect() {
    if (s_animate_led_task_handle != NULL) {
        BaseType_t outcome = xTaskNotifyIndexed(s_animate_led_task_handle, LedAnimationTaskNotificationIndex, LedAnimationTaskNotificationPause, eSetValueWithOverwrite);
        return outcome == pdPASS ? ESP_OK : ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t turn_led_string_on_off(led_string_state_t on) {
    switch (on) {
        case LedStringOn: {
            // Turn LEDs string on and clear all LEDs
            esp_err_t err = set_led_string_on_off(LedStringOn);
            if (err == ESP_OK) {
                // This calls refresh_led_string()
                return clear_led_string();
            }
        }
        break;

        case LedStringOff: {
            // Clear all LEDs - This calls refresh_led_string()
            esp_err_t err = clear_led_string();
            if (err == ESP_OK) {
                // Turn LEDs string off
                return set_led_string_on_off(LedStringOff);
            }
        }
        break;

        default:
            ESP_LOGE(LedStringTag, "Cannot turn LED string on/off - Unknown desired state (%d)", on);
        break;
    }

    return ESP_FAIL;
}

led_animation_task_notification_t accept_task_notification_with_delay(uint32_t delayMs) {
    uint32_t ulNotificationValue = 0UL;
    const TickType_t ticksToWait = delayMs != portMAX_DELAY ? pdMS_TO_TICKS(delayMs) : portMAX_DELAY;
    BaseType_t notificationWaitOutcome = xTaskNotifyWaitIndexed(LedAnimationTaskNotificationIndex, 0x0, 0x0, &ulNotificationValue, ticksToWait);
    ESP_LOGD(LedStringTag, "xTaskNotifyWaitIndexed() [Returned: %d] [Value: %lu] [Timeout: %lu]", notificationWaitOutcome, ulNotificationValue, delayMs);
    switch (notificationWaitOutcome) {
        case pdTRUE:
            // Notification received
            return ulNotificationValue;
        case pdFALSE:
            // Timeout - No notification was received
            return LedAnimationTaskNotificationNone;

        default:
            // Unknown notification - Log and ignore the unknown message
            ESP_LOGE(LedStringTag, "xTaskNotifyWaitIndexed() received unknown notification (%lu)", ulNotificationValue);
            return LedAnimationTaskNotificationNone;
    }
}

static void animate_led_task(void* arg) {
    led_animation_task_notification_t notification = LedAnimationTaskNotificationPause;
    for (;;) {
        switch (notification) {
            case LedAnimationTaskNotificationPause:
                // Wait (portMAX_DELAY = infinite timeout) to be awaken up to animate LEDs
                notification = accept_task_notification_with_delay(portMAX_DELAY);
#if CONFIG_HOLIDAYTREE_LEDS_LOG
                ESP_LOGI(LedStringTag, "animate_led_task() received notification '%s' (%d)", get_led_task_notification_name(notification), notification);
#endif
            break;

            default: {
                // Start desired effect
                if (notification >= LedAnimationTaskNotificationEffectMin) {
                    led_known_effects_t ledEffect = notification;

#if CONFIG_HOLIDAYTREE_LEDS_LOG
                    ESP_LOGI(LedStringTag, "animate_led_task() Trying to switch LED effect to '%s' (%d)", get_led_effect_name(ledEffect), notification);
#endif

                    if (ledEffect < LedEffectMax) {
                        turn_led_string_on_off(LedStringOn);
                        switch (ledEffect) {
                            case LedProgressiveRevealEffect:
                                notification = progressive_reveal_led_effect();
                            break;

                            default:
                                ESP_LOGW(LedStringTag, "animate_led_task() unable to start effect (%d) - Effect not implemented", ledEffect);
                            break;
                        }

#if CONFIG_HOLIDAYTREE_LEDS_LOG
                        ESP_LOGI(LedStringTag, "animate_led_task() effect exited with notification (%u)", notification);
#endif
                        turn_led_string_on_off(LedStringOff);
                    } else {
                        ESP_LOGE(LedStringTag, "animate_led_task() unable to start effect (%d) - Unknown effect", ledEffect);
                    }
                }
            }
            break;
        }
    }
}

static const char* get_led_task_notification_name(led_animation_task_notification_t notification) {
    switch (notification) {
        case LedAnimationTaskNotificationNone:
            return "LedAnimationTaskNotificationNone";
        case LedAnimationTaskNotificationPause:
            return "LedAnimationTaskNotificationPause";
        default:
            return get_led_effect_name(notification);
    }
}

static const char* get_led_effect_name(led_known_effects_t ledEffect) {
    switch (ledEffect) {
        case LedProgressiveRevealEffect:
            return "LedProgressiveRevealEffect";
        default:
            return "N/A";
    }
}