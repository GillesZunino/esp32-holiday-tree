// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

#include <esp_check.h>
#include <esp_log.h>


#include "bt/bt_avrc_volume.h"
#include "bt/i2s_output.h"


// I2S sound output log tags
static const char* BT_I2S_OUTPUT_TAG = "i2s_output";
static const char* BT_I2S_RINGBUFFER_TAG = "i2s_ringbuffer";


// Pins used to communicate with the I2S DAC / amplifier
static const gpio_num_t I2S_DATA_PIN = GPIO_NUM_25;
static const gpio_num_t I2S_BCK_PIN = GPIO_NUM_26;
static const gpio_num_t I2S_LRCK_PIN = GPIO_NUM_27;

// Ring buffer size - 32K is plenty as A2DP seems to be handing off data 4K bytes at a time from a2d_event_callback()
static const size_t RingBufferMaximumSizeInBytes = 32 * 1024;


// Number of bytes received from the A2DP callback (per call))
const static size_t A2DPBatchSizeInBytes = 4096; // 4K bytes

// Number of bytes to prefetch before releasing I2S write task
const static size_t MinimumPrefetchBufferSizeInBytes = 16 * 1024; // 16K bytes

// I2S task notification indices
const static UBaseType_t I2STaskBufferNotificationIndex = 0;

// I2S task notification values
typedef enum {
    NotificationNone = -1,
    NotificationBufferPrefilled = 0,
} I2SWriterNotificationType_t;

// Ring buffer mode of operation
typedef enum {
    RINGBUFFER_MODE_NONE = 0,           // Default
    RINGBUFFER_MODE_PREFETCHING = 1,    // Buffering incoming audio data - I2S is waiting
    RINGBUFFER_MODE_PROCESSING          // Buffering incoming audio data - I2S output writing to DMA
} RingBufferMode_t;


static i2s_chan_handle_t s_i2s_tx_channel = NULL;
static TaskHandle_t s_i2s_task_handle = NULL;
static RingbufHandle_t s_i2s_ringbuffer = NULL;
static RingBufferMode_t s_ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;

static size_t s_bytes_to_take_from_ringbuffer = 0;


static esp_err_t create_i2s_channel();
static esp_err_t delete_i2s_channel();

static esp_err_t start_i2s_output_task();
static esp_err_t stop_i2s_output_task();

static void i2s_task_handler(void* arg);

static I2SWriterNotificationType_t accept_i2s_task_notification_with_delay(uint32_t delayMs);
static esp_err_t notify_ringbuffer_prefilled();

static void get_dma_buffer_size_and_buffer_count_for_data_buffer_size(size_t batchSize, i2s_data_bit_width_t sampleBits, uint8_t channelCount, uint32_t* p_dma_desc_num, uint32_t* p_dma_frame_num, size_t* p_bytesToTakeFromRingBuffer);
static const char* get_ringbuffer_mode_name(RingBufferMode_t ringbufferMode);


esp_err_t create_i2s_output() {
    return create_i2s_channel();
}

esp_err_t start_i2s_output() {
    return start_i2s_output_task();
}

esp_err_t delete_i2s_output() {
    esp_err_t err = stop_i2s_output_task();
    if (err != ESP_OK) {
        ESP_LOGW(BT_I2S_OUTPUT_TAG, "stop_i2s_output_task() failed while shutting down I2S output task (%d)", err);
    }

    err = delete_i2s_channel();
    if (err != ESP_OK) {
        ESP_LOGW(BT_I2S_OUTPUT_TAG, "stop_i2s_output_task() failed while shutting down I2S channel (%d)", err);
    }

    return err;
}

esp_err_t configure_i2s_output(uint32_t sampleRate, i2s_data_bit_width_t dataWidth, i2s_slot_mode_t slotMode) {
    // Disable the transmission channel so it can be reconfigured
    ESP_RETURN_ON_ERROR(i2s_channel_disable(s_i2s_tx_channel), BT_I2S_OUTPUT_TAG, "i2s_channel_disable() failed");

    // Re-configure clock 
    i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
    ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_clock(s_i2s_tx_channel, &clk_cfg), BT_I2S_OUTPUT_TAG, "i2s_channel_reconfig_std_clock(%lu) failed", sampleRate);

    // Re-configure slot
    i2s_std_slot_config_t slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(dataWidth, slotMode);
    ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_slot(s_i2s_tx_channel, &slot_cfg), BT_I2S_OUTPUT_TAG, "i2s_channel_reconfig_std_slot(%d) failed", slotMode);

    // Enable the channel
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_i2s_tx_channel), BT_I2S_OUTPUT_TAG, "i2s_channel_enable");
    return ESP_OK;
}

static esp_err_t create_i2s_channel() {
    i2s_chan_config_t channel_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

    // Automatically send 0 on buffer empty
    channel_cfg.auto_clear = true;

    // DMA configuration - It is fixed at channel creation and cannot be changed later unless the channel is deleted
    get_dma_buffer_size_and_buffer_count_for_data_buffer_size(A2DPBatchSizeInBytes, I2S_DATA_BIT_WIDTH_16BIT, 2, &channel_cfg.dma_desc_num, &channel_cfg.dma_frame_num, &s_bytes_to_take_from_ringbuffer);

    // Standard configuration for I2S - Assume 44.1kHz - Frequency, sample size and number of channels can be changed without deleting the channel
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_PIN,
            .ws = I2S_LRCK_PIN,
            .dout = I2S_DATA_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(i2s_new_channel(&channel_cfg, &s_i2s_tx_channel, NULL), cleanup, BT_I2S_OUTPUT_TAG, "i2s_new_channel() failed");
    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_i2s_tx_channel, &std_cfg), cleanup, BT_I2S_OUTPUT_TAG, "i2s_channel_init_std_mode() failed");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(s_i2s_tx_channel), cleanup, BT_I2S_OUTPUT_TAG, "i2s_channel_enable() failed");

    return ESP_OK;

cleanup:
    if (s_i2s_tx_channel != NULL) {
        i2s_channel_disable(s_i2s_tx_channel);
        i2s_del_channel(s_i2s_tx_channel);
        s_i2s_tx_channel = NULL;
    }
    return ret;
}

static esp_err_t delete_i2s_channel() {
    esp_err_t err = ESP_OK;
    if (s_i2s_tx_channel != NULL) {
        if ((err = i2s_channel_disable(s_i2s_tx_channel)) != ESP_OK) {
            ESP_LOGW(BT_I2S_OUTPUT_TAG, "i2s_channel_disable() failed while shutting down I2S (%d)", err);
        }

        if ((err = i2s_del_channel(s_i2s_tx_channel)) != ESP_OK) {
            ESP_LOGW(BT_I2S_OUTPUT_TAG, "i2s_del_channel() failed while shutting down I2S (%d)", err);
        }

        s_i2s_tx_channel = NULL;
    }

    return err;
}

static esp_err_t start_i2s_output_task() {
    s_ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;

    ESP_LOGI(BT_I2S_OUTPUT_TAG, "Starting I2S output - %s", get_ringbuffer_mode_name(s_ringbuffer_mode));

    esp_err_t err = ESP_OK;

    // Create ring buffer
    s_i2s_ringbuffer = xRingbufferCreate(RingBufferMaximumSizeInBytes, RINGBUF_TYPE_BYTEBUF);
    if (s_i2s_ringbuffer == NULL) {
        ESP_LOGE(BT_I2S_OUTPUT_TAG, "xRingbufferCreate() failed");
        err = ESP_FAIL;
        goto cleanup;
    }

    // Create output task
    BaseType_t taskCreated = xTaskCreate(i2s_task_handler, "BtI2STask", 2048, NULL, configMAX_PRIORITIES - 3, &s_i2s_task_handle);
    err = taskCreated == pdPASS ? ESP_OK : ESP_FAIL;

cleanup:
    if (err != ESP_OK) {
        if (s_i2s_ringbuffer != NULL) {
            vRingbufferDelete(s_i2s_ringbuffer);
            s_i2s_ringbuffer = NULL;
        }
    }

    return err;
}

static esp_err_t stop_i2s_output_task() {
    if (s_i2s_task_handle != NULL) {
        vTaskDelete(s_i2s_task_handle);
        s_i2s_task_handle = NULL;
    }
    if (s_i2s_ringbuffer != NULL) {
        vRingbufferDelete(s_i2s_ringbuffer);
        s_i2s_ringbuffer = NULL;
    }

    return ESP_OK;
}

uint32_t write_to_i2s_output(const uint8_t* data, uint32_t size) {

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
    static uint64_t numberOfCalls = 0;

    numberOfCalls++;

    if (numberOfCalls % 100 == 0) {
        UBaseType_t bytesWaitingToBeRetrieved = 0;
        vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

        UBaseType_t freeBytes = RingBufferMaximumSizeInBytes - bytesWaitingToBeRetrieved;
        float percentOccupied = (100 * bytesWaitingToBeRetrieved) / RingBufferMaximumSizeInBytes;

        ESP_LOGI(BT_I2S_OUTPUT_TAG, "write_to_i2s_output() Ringbuffer stats - Waiting: %u bytes - Free: %u bytes - %f%% used", bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
    }
#endif

    BaseType_t ringBufferSendOutcome = xRingbufferSend(s_i2s_ringbuffer, (void *)data, size, (TickType_t) 20);
    if (ringBufferSendOutcome == pdTRUE) {
        if (s_ringbuffer_mode == RINGBUFFER_MODE_PREFETCHING) {
            size_t bytesWaitingToBeRetrieved = 0;
            vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
            if (bytesWaitingToBeRetrieved >= MinimumPrefetchBufferSizeInBytes) {
                s_ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
                esp_err_t err = notify_ringbuffer_prefilled();
                if (err != ESP_OK) {
                    ESP_LOGE(BT_I2S_RINGBUFFER_TAG, "write_to_i2s_output() - Unable to notify I2S task of pre-filled buffer");
                }
            }
        }
    } else {
        // Ring buffer is full or a timeout was encountered - This should never happen - Log an error and drop the batch
        ESP_LOGE(BT_I2S_RINGBUFFER_TAG, "write_to_i2s_output() - Ring buffer overflow - Dropping %lu bytes", size);
    }

    return ringBufferSendOutcome == pdTRUE ? size : 0;
}

static void i2s_task_handler(void* arg) {
    // Wait (portMAX_DELAY = infinite timeout) for the buffer to pre fill
    I2SWriterNotificationType_t notification = NotificationNone;
    do {
        notification = accept_i2s_task_notification_with_delay(portMAX_DELAY);
    } while (notification != NotificationBufferPrefilled);

    // Read from ring buffer - Write to I2S device
    for (;;) {
#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
        static uint64_t numberOfCalls = 0;
        numberOfCalls++;

        if (numberOfCalls % 100 == 0) {
            UBaseType_t bytesWaitingToBeRetrieved = 0;
            vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

            UBaseType_t freeBytes = RingBufferMaximumSizeInBytes - bytesWaitingToBeRetrieved;
            float percentOccupied = (100 * bytesWaitingToBeRetrieved) / RingBufferMaximumSizeInBytes;

            ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() DMA => Waiting %u - Free %u - %f%% used", bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
        }
#endif

        size_t sizeRetrievedFromRingBufferInBytes = 0;
        uint8_t *data = (uint8_t *) xRingbufferReceiveUpTo(s_i2s_ringbuffer, &sizeRetrievedFromRingBufferInBytes, (TickType_t) pdMS_TO_TICKS(20), s_bytes_to_take_from_ringbuffer);
        if (data != NULL) {
            size_t bytesWritten = 0;
            esp_err_t i2sWriteOutcome = i2s_channel_write(s_i2s_tx_channel, (void*) data, sizeRetrievedFromRingBufferInBytes, &bytesWritten, portMAX_DELAY);
            if (i2sWriteOutcome != ESP_OK) {
                ESP_LOGE(BT_I2S_OUTPUT_TAG, "i2s_task_handler() i2s_channel_write() failed with %d", i2sWriteOutcome);
            }
            vRingbufferReturnItem(s_i2s_ringbuffer, (void *) data);
        } else {
#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
            UBaseType_t bytesWaitingToBeRetrieved = 0;
            vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
            if (bytesWaitingToBeRetrieved > 0) {
                ESP_LOGW(BT_I2S_OUTPUT_TAG, "i2s_task_handler() Ring buffer data read timeout - In buffer waiting %u", bytesWaitingToBeRetrieved);
            }
#endif
        }
    }
}

static I2SWriterNotificationType_t accept_i2s_task_notification_with_delay(uint32_t delayMs) {
    uint32_t ulNotificationValue = 0UL;
    const TickType_t ticksToWait = delayMs != portMAX_DELAY ? pdMS_TO_TICKS(delayMs) : portMAX_DELAY;
    BaseType_t notificationWaitOutcome = xTaskNotifyWaitIndexed(I2STaskBufferNotificationIndex, 0x0, 0x0, &ulNotificationValue, ticksToWait);
    ESP_LOGD(BT_I2S_RINGBUFFER_TAG, "accept_i2s_task_notification_with_delay() - xTaskNotifyWaitIndexed() [Returned: %d] [Value: %lu] [Timeout: %lu]", notificationWaitOutcome, ulNotificationValue, delayMs);
    switch (notificationWaitOutcome) {
        case pdTRUE:
            // Notification received
            return ulNotificationValue;
        case pdFALSE:
            // Timeout - No notification was received
            return NotificationNone;
        default:
            // Unknown notification - Log and ignore the unknown message
            ESP_LOGE(BT_I2S_RINGBUFFER_TAG, "accept_i2s_task_notification_with_delay() - xTaskNotifyWaitIndexed() received unknown notification (%lu)", ulNotificationValue);
            return NotificationNone;
    }
}

static esp_err_t notify_ringbuffer_prefilled() {
    BaseType_t outcome = xTaskNotifyIndexed(s_i2s_task_handle, I2STaskBufferNotificationIndex, NotificationBufferPrefilled, eSetValueWithOverwrite);
    return outcome == pdPASS ? ESP_OK : ESP_FAIL;
}

static void get_dma_buffer_size_and_buffer_count_for_data_buffer_size(size_t batchSize, i2s_data_bit_width_t sampleBits, uint8_t channelCount, uint32_t* p_dma_desc_num, uint32_t* p_dma_frame_num, size_t* p_bytesToTakeFromRingBuffer) {
    //
    // I2S DMA buffer size (dma_frame_num) is expressed in sample count, not in bytes. This value must be such that (byte per sample * dma_frame_num) < = 4092 
    // I2S DMA buffer count (dma_desc_num) is usually => 2 and must be <= 511
    //
    const uint32_t FrameNumMaxInBytes = 4092;

    // Maximum number of samples which will fit in one DMA buffer given the <= 4092 requirement
    uint32_t bytesPerSample = ((sampleBits + 15) / 16) * 2 * channelCount;
    uint32_t maxSamplesCountAllowedForDMABufferSize = FrameNumMaxInBytes / bytesPerSample;

    // Calculate the number of samples in one batch
    uint32_t numberOfSamplesInBatch = batchSize / bytesPerSample;

    // Calculate the number of buffers to use - We try to allocate the bigget buffers possible - Larger DMA buffers are better because they yield fewer DMA interrupts to switch buffers
    if (numberOfSamplesInBatch <= maxSamplesCountAllowedForDMABufferSize) {
        // All samples in one batch fit directly in one DMA buffer - We request 2 times the number of buffers we need
        *p_dma_frame_num = numberOfSamplesInBatch;
        *p_dma_desc_num = 4;

        // We will take up to one DMA buffer worth of bytes from the ringbuffer per I2S write
        *p_bytesToTakeFromRingBuffer = numberOfSamplesInBatch * bytesPerSample;
    } else {
        // All samples in one batch do not fit in one DMA buffer - We request 2 times the number of buffers we need
        *p_dma_frame_num = maxSamplesCountAllowedForDMABufferSize;
        *p_dma_desc_num = 2 * ((numberOfSamplesInBatch / maxSamplesCountAllowedForDMABufferSize) + 1);

        // We will take up to one DMA buffer worth of bytes from the ringbuffer per I2S write
        *p_bytesToTakeFromRingBuffer = FrameNumMaxInBytes;
    }
    
    ESP_LOGI(BT_I2S_OUTPUT_TAG, "DMA dma_frame_num: %lu - DMA dma_desc_num: %lu - I2S write size: %u | Batch size %u, Sample size %d, Channels %d", *p_dma_frame_num, *p_dma_desc_num, *p_bytesToTakeFromRingBuffer, batchSize, sampleBits, channelCount);
}

static const char* get_ringbuffer_mode_name(RingBufferMode_t ringbufferMode) {
    switch (ringbufferMode) {
        case RINGBUFFER_MODE_NONE:
            return "RINGBUFFER_MODE_NONE";
        case RINGBUFFER_MODE_PREFETCHING:
            return "RINGBUFFER_MODE_PREFETCHING";
        case RINGBUFFER_MODE_PROCESSING:
            return "RINGBUFFER_MODE_PROCESSING";
        default:
            return "N/A";
    }
}