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


// LEDs animation task and current effect handle
static TaskHandle_t s_animate_led_task_handle = nullptr;
static led_effect_handle_t s_current_led_effect_handle = nullptr;


esp_err_t initialize_led_string_effect(led_effect_callback_t effect_fun, led_effect_handle_t* handle) {
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "NULL effect handle 'handle'");
    ESP_RETURN_ON_FALSE(effect_fun != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "NULL effect callback 'effect_fun'");

    // Allocate a new LED effect structure with default memory capabilities
    *handle = heap_caps_calloc(1, sizeof(struct led_effect), MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(*handle != NULL, ESP_ERR_NO_MEM, LedStringTag, "Could not allocate memory for LED effect");

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
    [[maybe_unused]] esp_err_t free_err = free_led_string_effect(pEffect);

#if CONFIG_HOLIDAYTREE_LEDS_LOG
    if (free_err != ESP_OK) {
        ESP_LOGE(LedStringTag, "Failed to free LED effect resources, error: (%" PRId32 ") -> %s", (int32_t) free_err, esp_err_to_name(free_err));
    }
#endif

    *handle = NULL;

    return ret;
}

esp_err_t free_led_string_effect(led_effect_handle_t handle) {
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "NULL effect handle 'handle'");
    ESP_RETURN_ON_FALSE(s_current_led_effect_handle != handle, ESP_ERR_INVALID_STATE, LedStringTag, "Cannot free the currently running LED effect");

    // Delete the event group associated with the LED effect task, if it exists
    if (handle->led_task_event_group_handle != NULL) {
        vEventGroupDelete(handle->led_task_event_group_handle);
        handle->led_task_event_group_handle = NULL;
    }

    heap_caps_free(handle);

    return ESP_OK;
}

esp_err_t start_led_string_effect(led_effect_handle_t handle, void* context) {
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "NULL effect handle 'handle'");
    ESP_RETURN_ON_FALSE(handle->effect_cb != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "NULL effect callback");
    ESP_RETURN_ON_FALSE(handle->led_task_event_group_handle != NULL, ESP_ERR_INVALID_STATE, LedStringTag, "NULL event group handle");

    ESP_RETURN_ON_FALSE((s_current_led_effect_handle == NULL) && (s_animate_led_task_handle == NULL),
                        ESP_ERR_INVALID_STATE, LedStringTag, "Cannot start a new effect before stopping the currently running one");

    // Publish the handle and user context before spawning the task so the task
    // never observes a partially initialized effect
    handle->user_context = context;
    s_current_led_effect_handle = handle;

    // Ensure any event group bit is cleared before starting the effect
    xEventGroupClearBits(handle->led_task_event_group_handle, EffectTaskEndedEventGroupBit | EffectEndEventGroupBit);

    // Spawn the task to run the LED animation effect
    BaseType_t outcome = xTaskCreate(animate_led_task, "ht-leds-anim", CONFIG_HOLIDAYTREE_LED_ANIMATOR_TASK_STACK_SIZE, handle, CONFIG_HOLIDAYTREE_LED_ANIMATOR_TASK_PRIORITY, &s_animate_led_task_handle);
    if (outcome != pdPASS) {
        s_animate_led_task_handle = NULL;
        s_current_led_effect_handle = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t stop_current_led_string_effect(void) {
    ESP_RETURN_ON_FALSE(s_current_led_effect_handle != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "No currently running LED effect");

    return stop_led_string_effect(s_current_led_effect_handle);
}


static esp_err_t stop_led_string_effect(led_effect_handle_t handle) {
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, LedStringTag, "NULL effect handle 'handle'");

    esp_err_t err = ESP_OK;
    
    // Signal the LED effect to stop and wait for the task to shutdown
    if (s_animate_led_task_handle != NULL) {
        err = notify_led_effect_stop_and_wait_for_shutdown(handle);
    }

    s_animate_led_task_handle = NULL;
    s_current_led_effect_handle = NULL;

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
        ESP_LOGI(LedStringTag, "Effect received notification: (%" PRId8 ") -> %s", notification, get_led_task_notification_name(notification));
    }
#endif

    return notification;
}


static esp_err_t clear_led_string_and_turn_on_off(bool on) {
    if (on) {
        // Turn LEDs string on and clear all LEDs
        ESP_RETURN_ON_ERROR(set_led_string_on_off(true), LedStringTag, "Failed to set LED string on");
        // This calls refresh_led_string()
        return clear_led_string();
    } else {
        // Clear all LEDs - This calls refresh_led_string()
        ESP_RETURN_ON_ERROR(clear_led_string(), LedStringTag, "Failed to clear LED string");
        // Turn LEDs string off
        return set_led_string_on_off(false);
    }

    return ESP_OK;
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