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
const UBaseType_t ledAnimationTaskNotificationIndex = 0;

// LEDs animation task
static TaskHandle_t xAnimateLEDTaskHandle = NULL;


esp_err_t start_led_strip_effect(LEDEffect_t led_effect) {
    if (xAnimateLEDTaskHandle == NULL) {
        BaseType_t outcome = xTaskCreate(animate_led_task, "leds_animate", 3072, NULL, 10, &xAnimateLEDTaskHandle);
        if (outcome != pdPASS) {
            xAnimateLEDTaskHandle = NULL;
            return ESP_FAIL;
        }
    }

    BaseType_t outcome = xTaskNotifyIndexed(xAnimateLEDTaskHandle, ledAnimationTaskNotificationIndex, led_effect, eSetValueWithOverwrite);
    return outcome == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t stop_led_strip_effect(void) {
    if (xAnimateLEDTaskHandle != NULL) {
        BaseType_t outcome = xTaskNotifyIndexed(xAnimateLEDTaskHandle, ledAnimationTaskNotificationIndex, NotificationPause, eSetValueWithOverwrite);
        return outcome == pdPASS ? ESP_OK : ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t turn_led_string_on_off(led_string_state_t on) {
    switch (on) {
        case ON: {
            // Turn LEDs string on and clear all LEDs
            esp_err_t err = set_led_string_on_off(ON);
            if (err == ESP_OK) {
                // This calls refresh_led_string()
                return clear_led_string();
            }
        }
        break;

        case OFF: {
            // Clear all LEDs - This calls refresh_led_string()
            esp_err_t err = clear_led_string();
            if (err == ESP_OK) {
                // Turn LEDs string off
                return set_led_string_on_off(OFF);
            }
        }
        break;

        default:
            ESP_LOGE(LED_TAG, "Cannot turn LED strip on/off - Unknown desired state (%d)", on);
        break;
    }

    return ESP_FAIL;
}

NotificationType_t accept_task_notification_with_delay(uint32_t delayMs) {
    uint32_t ulNotificationValue = 0UL;
    const TickType_t ticksToWait = delayMs != portMAX_DELAY ? pdMS_TO_TICKS(delayMs) : portMAX_DELAY;
    BaseType_t notificationWaitOutcome = xTaskNotifyWaitIndexed(ledAnimationTaskNotificationIndex, 0x0, 0x0, &ulNotificationValue, ticksToWait);
    ESP_LOGD(LED_TAG, "xTaskNotifyWaitIndexed() [Returned: %d] [Value: %lu] [Timeout: %lu]", notificationWaitOutcome, ulNotificationValue, delayMs);
    switch (notificationWaitOutcome) {
        case pdTRUE:
            // Notification received
            return ulNotificationValue;
        case pdFALSE:
            // Timeout - No notification was received
            return NotificationNone;

        default:
            // Unknown notification - Log and ignore the unknown message
            ESP_LOGE(LED_TAG, "xTaskNotifyWaitIndexed() received unknown notification (%lu)", ulNotificationValue);
            return NotificationNone;
    }
}

static void animate_led_task(void* arg) {
    NotificationType_t notification = NotificationPause;
    for (;;) {
        switch (notification) {
            case NotificationPause:
                // Wait (portMAX_DELAY = infinite timeout) to be awaken up to animate LEDs
                notification = accept_task_notification_with_delay(portMAX_DELAY);
                ESP_LOGI(LED_TAG, "animate_led_task() received notification (%d)", notification);
            break;

            default: {
                // Start desired effect
                if (notification >= NotificationEffectMin) {
                    LEDEffect_t led_effect = notification;
                    if (led_effect < LEDEffectMax) {
                        turn_led_string_on_off(ON);
                        switch (led_effect) {
                            case ProgressiveReveal:
                                notification = progressive_reveal_led_effect();
                            break;

                            default:
                                ESP_LOGW(LED_TAG, "animate_led_task() unable to start effect (%d) - Effect not implemented", led_effect);
                            break;
                        }
                        ESP_LOGI(LED_TAG, "animate_led_task() effect exited with notification (%u)", notification);
                        turn_led_string_on_off(OFF);
                    } else {
                        ESP_LOGE(LED_TAG, "animate_led_task() unable to start effect (%d) - Unknown effect", led_effect);
                    }
                }
            }
            break;
        }
    }
}