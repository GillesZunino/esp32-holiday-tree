// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_check.h>

#include "leds_animator/led_animator.h"
#include "leds_animator/led_internals.h"


// Event bit used to signal the end of an LED effect
const EventBits_t EffectTaskEndedEventGroupBit = 1 << 0;    // Set by the LED effect task to signal it ended
const EventBits_t EffectEndEventGroupBit = 1 << 1;          // Set by the animator to signal the LED effect task to end


// Handle structure representing an LED effect
struct led_effect {
    EventGroupHandle_t led_task_event_group_handle;
    led_effect_callback_t effect_cb;
    void* user_context;
};


static esp_err_t stop_led_string_effect(led_effect_handle_t handle);
static esp_err_t notify_led_effect_stop_and_wait_for_shutdown(led_effect_handle_t handle);

static void animate_led_task(void* arg);
static led_effect_notification_t delay_with_parent_signals(TickType_t xTicksToWait);

static esp_err_t clear_led_string_and_turn_on_off(bool on);

#if CONFIG_HOLIDAYTREE_LEDS_LOG
static const char* get_led_task_notification_name(led_effect_notification_t notification);
#endif


// LEDs animation task
static TaskHandle_t s_animate_led_task_handle = nullptr;
static led_effect_handle_t s_current_led_effect_handle = nullptr;


esp_err_t initialize_led_string_effect(led_effect_callback_t effect_fun, led_effect_handle_t* handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate a new LED effect structure with default memory capabilities
    *handle = heap_caps_calloc(1, sizeof(struct led_effect), MALLOC_CAP_DEFAULT);
    if (*handle == NULL) {
        return ESP_ERR_NO_MEM;
    }

    led_effect_handle_t pEffect = *handle;
    esp_err_t ret = ESP_OK;

    // Initialize blittable fields
    pEffect->effect_cb = effect_fun;
    pEffect->user_context = NULL;

    // Initialize an Event Group to communicate with the effect task
    pEffect->led_task_event_group_handle = xEventGroupCreate();
    ESP_GOTO_ON_FALSE(pEffect->led_task_event_group_handle != NULL, ESP_ERR_NO_MEM, cleanup, LedStringTag, "Could not allocate memory for event group");

    return ESP_OK;

cleanup:
#if CONFIG_HOLIDAYTREE_LEDS_LOG
    esp_err_t free_err = 
#endif
    free_led_string_effect(pEffect);

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    ESP_LOGE(LedStringTag, "Failed to free LED effect resources, error: (%d) -> %s", free_err, esp_err_to_name(free_err));
#endif

    return ret;
}

esp_err_t free_led_string_effect(led_effect_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Cannot free the currently running LED effect
    if (s_current_led_effect_handle == handle) {
        return ESP_ERR_INVALID_STATE;
    }

    // Delete the event group associated with the LED effect task, if it exists
    if (handle->led_task_event_group_handle != NULL) {
        vEventGroupDelete(handle->led_task_event_group_handle);
        handle->led_task_event_group_handle = NULL;
    }

    heap_caps_free(handle);

    return ESP_OK;
}

esp_err_t start_led_string_effect(led_effect_handle_t handle, void* context) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Cannot start a new effect before stopping the currently running one
    if ((s_current_led_effect_handle != NULL) || (s_animate_led_task_handle != NULL)) {
        return ESP_ERR_INVALID_STATE;
    }

    // Ensure any event group bit is cleared before starting the effect
    xEventGroupClearBits(handle->led_task_event_group_handle, EffectTaskEndedEventGroupBit | EffectEndEventGroupBit);

    // Spawn the task to run the LED animation effect
    BaseType_t outcome = xTaskCreate(animate_led_task, "ht-leds-anim", 3072, handle, 10, &s_animate_led_task_handle);
    if (outcome != pdPASS) {
        s_animate_led_task_handle = NULL;
        return ESP_FAIL;
    }

    handle->user_context = context;
    s_current_led_effect_handle = handle;

    return ESP_OK;
}

esp_err_t stop_current_led_string_effect(void) {
    if (s_current_led_effect_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return stop_led_string_effect(s_current_led_effect_handle);
}


static esp_err_t stop_led_string_effect(led_effect_handle_t handle) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ESP_OK;
    
    // Signal the LED effect to stop and wait for the task to shutdown
    if (s_animate_led_task_handle != NULL) {
        err = notify_led_effect_stop_and_wait_for_shutdown(handle);
    }

    s_animate_led_task_handle = nullptr;
    s_current_led_effect_handle = nullptr;

    return err;
}

static esp_err_t notify_led_effect_stop_and_wait_for_shutdown(led_effect_handle_t handle) {
    // Tell the effect to shutdown and wait for the task to confirm it has ended
    xEventGroupSync(handle->led_task_event_group_handle, EffectEndEventGroupBit, EffectTaskEndedEventGroupBit, portMAX_DELAY);

    // Clean up the event group bits
    xEventGroupClearBits(handle->led_task_event_group_handle, EffectEndEventGroupBit | EffectTaskEndedEventGroupBit);

    return ESP_OK;
}


static void animate_led_task(void* arg) {
    led_effect_handle_t currentEffect = (led_effect_handle_t) arg;

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    ESP_LOGI(LedStringTag, "animate_led_task() starting");
#endif

    led_effect_context_t context = {
        .context = currentEffect->user_context,
        .animator_cb = {
            .set_led_string_on_off = set_led_string_on_off,
            .set_led_string_pixel = set_led_string_pixel,
            .refresh_led_string = refresh_led_string,
            .clear_led_string = clear_led_string,
            .delay_with_parent_signals = delay_with_parent_signals,
        }
    };

    // Turn strip on and clear it
    clear_led_string_and_turn_on_off(true);

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    ESP_LOGI(LedStringTag, "animate_led_task() effect starting");
#endif

    // Invoke the LED effect callback with the prepared context
    currentEffect->effect_cb(&context);

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    ESP_LOGI(LedStringTag, "animate_led_task() effect ending");
#endif

    // Clear strip and turn it off
    clear_led_string_and_turn_on_off(false);

    // Signal that the LED effect task has ended
    xEventGroupSetBits(currentEffect->led_task_event_group_handle, EffectTaskEndedEventGroupBit);

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    ESP_LOGI(LedStringTag, "animate_led_task() ending");
#endif

    vTaskDelete(NULL);
}

static led_effect_notification_t delay_with_parent_signals(TickType_t xTicksToWait) {
    led_effect_notification_t notification = LedEffectNotificationNone;
    EventBits_t eventBits = xEventGroupWaitBits(s_current_led_effect_handle->led_task_event_group_handle, EffectEndEventGroupBit, pdTRUE, pdFALSE, xTicksToWait);
    if ((eventBits & EffectEndEventGroupBit) == EffectEndEventGroupBit) {
        notification = LedEffectNotificationEnd;
    }

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    if (notification != LedEffectNotificationNone) {
        ESP_LOGI(LedStringTag, "Effect received notification: (%d) -> %s", notification, get_led_task_notification_name(notification));
    }
#endif

    return notification;
}


static esp_err_t clear_led_string_and_turn_on_off(bool on) {
    if (on) {
        // Turn LEDs string on and clear all LEDs
        esp_err_t err = set_led_string_on_off(true);
        if (err == ESP_OK) {
            // This calls refresh_led_string()
            return clear_led_string();
        }
    } else {
        // Clear all LEDs - This calls refresh_led_string()
        esp_err_t err = clear_led_string();
        if (err == ESP_OK) {
            // Turn LEDs string off
            return set_led_string_on_off(false);
        }
    }

    return ESP_FAIL;
}


#if CONFIG_HOLIDAYTREE_LEDS_LOG

static const char* get_led_task_notification_name(led_effect_notification_t notification) {
    switch (notification) {
        case LedEffectNotificationNone:
            return "LedEffectNotificationNone";
        case LedEffectNotificationEnd:
            return "LedEffectNotificationEnd";
        default:
            return "N/A";
    }
}

#endif