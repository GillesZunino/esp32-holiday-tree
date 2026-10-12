// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_check.h>


#include "gpio_init.h"


static const char* GpioIsrTag = "gpio_isr";

static QueueHandle_t s_gpio_isr_dispatch_queue = NULL;


esp_err_t gpio_events_queue_dispatch(void) {
    QueueHandle_t dispatchQueue = s_gpio_isr_dispatch_queue;
    ESP_RETURN_ON_FALSE(dispatchQueue != NULL, ESP_ERR_INVALID_STATE, GpioIsrTag, "gpio_events_queue_dispatch() called with NULL queue - call configure_gpio_isr_dispatcher() first");

    for(;;) {
        isr_handler_fn_ptr handlerFn = NULL;
        BaseType_t dequeueOutcome = xQueueReceive(dispatchQueue, &handlerFn, portMAX_DELAY);
        if (dequeueOutcome == pdTRUE) {
            if (handlerFn != NULL) {
                handlerFn();
            } else {
                ESP_LOGE(GpioIsrTag, "xQueueReceive() retrieved an ISR request with NULL handler");
            }
        } else {
            ESP_LOGE(GpioIsrTag, "xQueueReceive() failed (0x%" PRIx32 ") - ISR will be dropped", (uint32_t) dequeueOutcome);
        }
    }
    return ESP_OK;
}

static void gpio_isr_handler(void* arg) {
    isr_handler_fn_ptr handlerFn = (isr_handler_fn_ptr) arg;
    ESP_RETURN_VOID_ON_FALSE_ISR(handlerFn != NULL, GpioIsrTag, "gpio_isr_handler() called with NULL handler - ISR will be dropped");
    QueueHandle_t dispatchQueue = s_gpio_isr_dispatch_queue;
    ESP_RETURN_VOID_ON_FALSE_ISR(dispatchQueue != NULL, GpioIsrTag, "gpio_isr_handler() called with NULL queue - ISR will be dropped");
    
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    BaseType_t enqueueOutcome = xQueueSendFromISR(dispatchQueue, &handlerFn, &higherPriorityTaskWoken);
    if (enqueueOutcome != pdPASS) {
        ESP_DRAM_LOGE(GpioIsrTag, "xQueueSendFromISR() failed (0x%" PRIx32 ") - ISR will be dropped", (uint32_t) enqueueOutcome);
    } else if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t configure_gpio_isr_queue() {
    ESP_RETURN_ON_FALSE(s_gpio_isr_dispatch_queue == NULL, ESP_ERR_INVALID_STATE, GpioIsrTag, "GPIO ISR queue already configured");
    s_gpio_isr_dispatch_queue = xQueueCreate(10, sizeof(isr_handler_fn_ptr));
    return s_gpio_isr_dispatch_queue != NULL ? ESP_OK : ESP_FAIL;
}

static esp_err_t shutdown_gpio_isr_queue() {
    if (s_gpio_isr_dispatch_queue != NULL) {
        vQueueDelete(s_gpio_isr_dispatch_queue);
        s_gpio_isr_dispatch_queue = NULL;
    }

    return ESP_OK;
}

esp_err_t ht_gpio_isr_handler_add(gpio_num_t gpioNum, isr_handler_fn_ptr fn) {
    ESP_RETURN_ON_FALSE(fn != NULL, ESP_ERR_INVALID_ARG, GpioIsrTag, "NULL ISR handler 'fn'");
    return gpio_isr_handler_add(gpioNum, gpio_isr_handler, (void*) fn);
}

esp_err_t ht_gpio_isr_handler_delete(gpio_num_t gpioNum) {
    return gpio_isr_handler_remove(gpioNum);
}

esp_err_t configure_gpio_isr_dispatcher(void) {
    ESP_ERROR_CHECK(configure_gpio_isr_queue());

    //
    // Installing the GPIO ISR Service depends on IPC tasks - See https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32/api-reference/system/ipc.html
    // The following configuration values are involved:
    // * CONFIG_ESP_IPC_USES_CALLERS_PRIORITY - Default on
    // * CONFIG_ESP_IPC_TASK_STACK_SIZE       - Default 1024 - Raised to 1280 (0x500) to avoid stack overflow in ipc0 (see sdkconfig.defaults)
    //
    const int ESP_INTR_FLAG_NONE = 0;
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_NONE);
    if (err != ESP_OK) {
        if (shutdown_gpio_isr_queue() != ESP_OK) {
            ESP_LOGE(GpioIsrTag, "gpio_install_isr_service() failed and cleanup action shutdown_gpio_isr_queue() failed (0x%" PRIx32 ")", (uint32_t) err);
        }
    }

    return err;
}