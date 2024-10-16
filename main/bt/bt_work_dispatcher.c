// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include "bt/bt_work_dispatcher.h"



// Bluetooth event work queue log tag
static const char* BtWorkQueueTag = "bt_workqueue";


// A unit of work which can be enqueued and executed
typedef struct {
    uint16_t                    eventId;
    bluetooth_workitem_handler  handler;
    void*                       params;
} bluetooth_workitem_t;


static QueueHandle_t s_bt_app_task_queue = NULL;
static TaskHandle_t s_bt_app_task_handle = NULL;


static bool queue_workitem_internal(bluetooth_workitem_t* workItem);
static void queue_consumer_task(void* arg);


bool queue_bluetooth_workitem(bluetooth_workitem_handler handler, uint16_t eventId, const void* const params, size_t paramsLen) {
    ESP_LOGD(BtWorkQueueTag, "%s() event: 0x%x, param len: %d", __func__, eventId, paramsLen);

    if ((s_bt_app_task_queue != NULL) && (s_bt_app_task_handle != NULL)) {
        bluetooth_workitem_t workItem = {
            .eventId = eventId,
            .handler = handler
        };

        if (paramsLen == 0) {
            return queue_workitem_internal(&workItem);
        } else {
            if ((params != NULL) && (paramsLen > 0)) {
                void* stagedParams = malloc(paramsLen);
                if (stagedParams != NULL) {
                    memcpy(stagedParams, params, paramsLen);
                    workItem.params = stagedParams;

                    bool workQueued = queue_workitem_internal(&workItem);

                    if (!workQueued) {
                        free(stagedParams);
                    }

                    return workQueued;
                }
            }
        }
    } else {
        ESP_LOGE(BtWorkQueueTag, "%s() trying to queue event 0x%x when dispatcher is not running", __func__, eventId);
    }

    return false;
}

esp_err_t start_bluetooth_dispatcher_task() {
    const UBaseType_t workItemDepth = 10;

    s_bt_app_task_queue = xQueueCreate(workItemDepth, sizeof(bluetooth_workitem_t));
    if (s_bt_app_task_queue == NULL) {
        ESP_LOGE(BtWorkQueueTag, "%s() xQueueCreate() failed", __func__);
        return ESP_FAIL;
    }

    // Run the I2S task on the non Bluetooth core
    const BaseType_t appCoreId = CONFIG_BT_BLUEDROID_PINNED_TO_CORE == PRO_CPU_NUM ? APP_CPU_NUM : PRO_CPU_NUM;
    BaseType_t taskCreated = xTaskCreatePinnedToCore(queue_consumer_task, "ht-BT-dispatch", 2560, NULL, 10, &s_bt_app_task_handle, appCoreId);
    if (taskCreated != pdPASS) {
        ESP_LOGE(BtWorkQueueTag, "%s() xTaskCreate() failed", __func__);

        vQueueDelete(s_bt_app_task_queue);
        s_bt_app_task_queue = NULL;
        s_bt_app_task_handle = NULL;

        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t stop_bluetooth_dispatcher_task() {
    if (s_bt_app_task_handle != NULL) {
        vTaskDelete(s_bt_app_task_handle);
        s_bt_app_task_handle = NULL;
    }

    if (s_bt_app_task_queue != NULL) {
        bool queueHasItems = true;
        do {
            bluetooth_workitem_t workItem;
            queueHasItems = xQueueReceive(s_bt_app_task_queue, &workItem, 0) == pdTRUE;
            if (queueHasItems) {
                if (workItem.params != NULL) {
                    free(workItem.params);
                }
            }
        } while (queueHasItems);

        vQueueDelete(s_bt_app_task_queue);
        s_bt_app_task_queue = NULL;
    }

    return ESP_OK;
}

static bool queue_workitem_internal(bluetooth_workitem_t* workItem) {
    bool enqueued = xQueueSendToBack(s_bt_app_task_queue, workItem, pdMS_TO_TICKS(10)) == pdTRUE;
    if (!enqueued) {
        ESP_LOGE(BtWorkQueueTag, "%s xQueueSendToBack() failed", __func__);
    }

    return enqueued;
}

static void queue_consumer_task(void* arg) {
    for (;;) {
        bluetooth_workitem_t workItem;
        bool itemDequeued = xQueueReceive(s_bt_app_task_queue, &workItem, (TickType_t) portMAX_DELAY) == pdTRUE;
        if (itemDequeued) {
            ESP_LOGD(BtWorkQueueTag, "%s, dequeued event: 0x%x", __func__, workItem.eventId);

            if (workItem.handler != NULL) {
                workItem.handler(workItem.eventId, workItem.params);
            }

            if (workItem.params != NULL) {
                free(workItem.params);
            }
        }
    }
}