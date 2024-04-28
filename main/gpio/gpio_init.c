// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_check.h>


#include "gpio_init.h"


static const char* GPIO_ISR_TAG = "gpio_isr";

static QueueHandle_t s_gpio_isr_dispatch_queue = NULL;
static TaskHandle_t s_gpio_isr_taskHandle = NULL;


static void gpio_isr_handler(void* arg) {
    isr_handler_fn_ptr handlerFn = (isr_handler_fn_ptr) arg;
    BaseType_t enqueuOutcome = xQueueGenericSendFromISR(s_gpio_isr_dispatch_queue, &handlerFn, NULL, queueSEND_TO_BACK);
    if (enqueuOutcome != pdPASS) {
        ESP_LOGE(GPIO_ISR_TAG, "xQueueGenericSendFromISR() failed (0x%x) - ISR will be dropped", enqueuOutcome);
    }
}

static void gpio_isr_dispatch_task(void* arg) {
    for(;;) {
        isr_handler_fn_ptr handlerFn = NULL;
        BaseType_t dequeueOutcome = xQueueReceive(s_gpio_isr_dispatch_queue, &handlerFn, portMAX_DELAY);
        if (dequeueOutcome == pdTRUE) {
            if (handlerFn != NULL) {
                handlerFn();
            } else {
                ESP_LOGE(GPIO_ISR_TAG, "xQueueReceive() retrieved and ISR request with NULL handler");
            }
        } else {
            ESP_LOGE(GPIO_ISR_TAG, "xQueueReceive() failed (0x%x) - ISR will be dropped", dequeueOutcome);
        }
    }
}

static esp_err_t configure_isr_task(void) {
    s_gpio_isr_dispatch_queue = xQueueCreate(10, sizeof(void *));
    if (s_gpio_isr_dispatch_queue != NULL) {
        BaseType_t taskCreateOutcome = xTaskCreate(gpio_isr_dispatch_task, "GPIO ISR Task", 2048, NULL, 10, &s_gpio_isr_taskHandle);
        if (taskCreateOutcome == pdTRUE) {
            return ESP_OK;
        } else {
            vQueueDelete(s_gpio_isr_dispatch_queue);
            s_gpio_isr_dispatch_queue = NULL;
            return ESP_FAIL;
        }
    }

    return ESP_FAIL;
}

esp_err_t ht_gpio_isr_handler_add(gpio_num_t gpio_num, isr_handler_fn_ptr fn) {
    if (fn != NULL) {
        return gpio_isr_handler_add(gpio_num, gpio_isr_handler, (void*) fn);
    } else {
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t ht_gpio_isr_handler_delete(gpio_num_t gpio_num) {
    return gpio_isr_handler_remove(gpio_num);
}

esp_err_t configure_gpio_isr_dispatcher(void) {
    ESP_ERROR_CHECK(configure_isr_task());

    //
    // Installing the GPIO ISR Service depends on IPC tasks - See https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32/api-reference/system/ipc.html
    // The following configuration values are involved:
    // * CONFIG_ESP_IPC_USES_CALLERS_PRIORITY - Default on
    // * CONFIG_ESP_IPC_TASK_STACK_SIZE       - Default 1024 - Raised to 1280 (0x500) to avoid stack overflow in ipc0 (see sdkconfig.defaults)
    //
    const int ESP_INTR_FLAG_NONE = 0;
    return gpio_install_isr_service(ESP_INTR_FLAG_NONE);
}