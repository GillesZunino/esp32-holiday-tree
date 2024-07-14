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
const static UBaseType_t I2STaskNotificationIndex = 0;

// I2S task notification values
typedef enum {
    NotificationNone = -1,
    NotificationAudioActive = 1,
    NotificationAudioPaused = 2
} I2SWriterNotificationType_t;

// Ring buffer mode of operation
typedef enum {
    RingbufferNone = 0,
    RingbufferPaused = 1,       // Audio paused
    RingbufferPrefetching = 2,  // Buffering incoming audio data - I2S is waiting
    RingbufferWriting = 3       // Buffering incoming audio data - I2S output writing to DMA
} RingBufferMode_t;


static i2s_chan_handle_t s_i2s_tx_channel = NULL;
static TaskHandle_t s_i2s_task_handle = NULL;
static RingbufHandle_t s_i2s_ringbuffer = NULL;

static size_t s_bytes_to_take_from_ringbuffer = 0;


static esp_err_t create_i2s_channel();
static esp_err_t delete_i2s_channel();

static esp_err_t start_i2s_output_task();
static esp_err_t stop_i2s_output_task();

static void i2s_task_handler(void* arg);

static esp_err_t take_from_ringbuffer_and_write_to_i2s(size_t maxBytesToTakeFromBuffer, TickType_t readMaxWaitInTicks, size_t* pBytesTakenFromBuffer);
static I2SWriterNotificationType_t accept_i2s_task_notification_with_delay(uint32_t delayMs);

static esp_err_t notify_a2dp_audio_active();
static esp_err_t notify_a2dp_audio_paused();
static esp_err_t notify_i2s_task(I2SWriterNotificationType_t notificationType);

static void get_dma_buffer_size_and_buffer_count_for_data_buffer_size(size_t batchSize, i2s_data_bit_width_t sampleBits, uint8_t channelCount, uint32_t* p_dma_desc_num, uint32_t* p_dma_frame_num, size_t* p_bytesToTakeFromRingBuffer);
static const char* get_ringbuffer_mode_name(RingBufferMode_t ringbufferMode);
static const char* get_i2s_task_notificationType(I2SWriterNotificationType_t notificationType);


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
    ESP_LOGI(BT_I2S_OUTPUT_TAG, "Starting I2S output");

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

esp_err_t set_i2s_output_audio_state(esp_a2d_audio_state_t audioState) {
    switch (audioState) {
        case ESP_A2D_AUDIO_STATE_SUSPEND:
            return notify_a2dp_audio_paused();

        case ESP_A2D_AUDIO_STATE_STARTED:
            return notify_a2dp_audio_active();

        default:
            ESP_LOGE(BT_I2S_RINGBUFFER_TAG, "set_i2s_output_audio_state() - Unable to notify I2S task of audio state %d", audioState);
            return ESP_FAIL;
    }
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

        ESP_LOGI(BT_I2S_OUTPUT_TAG, "write_to_i2s_output() Writing %lu bytes | Ringbuffer stats - Waiting: %u bytes - Free: %u bytes - %f%% used", size, bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
    }
#endif

    const uint8_t MaxTries = 5;
    const TickType_t WriteWaitTimeInTicks = 5;
    const TickType_t WaitTimeInTicks = 2;

    bool shouldTry = true;
    uint8_t tries = 0;
    do {
        BaseType_t ringBufferSendOutcome = xRingbufferSend(s_i2s_ringbuffer, (void *)data, size, WriteWaitTimeInTicks);
        if (ringBufferSendOutcome == pdTRUE) {
            return size;
        }
        else {
            tries++;
            shouldTry = tries <= MaxTries;

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
            ESP_LOGE(BT_I2S_RINGBUFFER_TAG, "write_to_i2s_output() - Timed out / Failed to write to ring buffer - %s (%d)", shouldTry ? "RETRY" : "NO RETRY", tries);
#endif

            if (shouldTry) {
                vTaskDelay(WaitTimeInTicks);
            }
        }
    } while (shouldTry);

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
    ESP_LOGE(BT_I2S_RINGBUFFER_TAG, "write_to_i2s_output() - Timed out trying to write to ring buffer or ring buffer overflow - Dropped %lu bytes", size);
#endif

    return 0;
}

static void i2s_task_handler(void* arg) {
    RingBufferMode_t currentMode = RingbufferNone;

    for (;;) {
        uint32_t notificationDelay = (currentMode == RingbufferWriting) || (currentMode == RingbufferPrefetching) ? 5 : portMAX_DELAY;
        I2SWriterNotificationType_t notification = accept_i2s_task_notification_with_delay(notificationDelay);

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
    if (notification != NotificationNone) {
        ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [%s] - Received notification '%s'", get_ringbuffer_mode_name(currentMode), get_i2s_task_notificationType(notification));
    }
#endif

        switch (notification) {
            case NotificationNone:
            break;

            case NotificationAudioActive:{
                switch (currentMode) {
                    case RingbufferNone:
                    case RingbufferPaused: {
#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
                        ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [%s] - New mode 'RingbufferPrefetching'", get_ringbuffer_mode_name(currentMode));
#endif
                        currentMode = RingbufferPrefetching;
                    }
                    break;

                    case RingbufferPrefetching:
                    case RingbufferWriting:
                    break;

                    default: 
                        ESP_LOGE(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [NotificationAudioActive] Unhandled mode of operation %s (%d)", get_ringbuffer_mode_name(currentMode), currentMode);
                    break;
                }
            }
            break;

            case NotificationAudioPaused: {
                switch (currentMode) {
                    case RingbufferNone:
                    case RingbufferPaused:
                        currentMode = RingbufferPaused;
                    break;

                    case RingbufferPrefetching:
                    case RingbufferWriting: {
                        ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [%s] - Draining buffer before switching to 'RingbufferPaused' mode", get_ringbuffer_mode_name(currentMode));

                        bool shouldWrite = true;
                        do {
                            const TickType_t DrainWaitTimeInTicks = 10;
                            size_t takenFromBufferInBytes = 0;
                            esp_err_t err = take_from_ringbuffer_and_write_to_i2s(s_bytes_to_take_from_ringbuffer, DrainWaitTimeInTicks, &takenFromBufferInBytes);
                            switch (err) {
                                case ESP_OK:
#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
                                    ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferWriting] - Drained %u bytes",  takenFromBufferInBytes);
#endif
                                break;

                                case ESP_ERR_TIMEOUT: {
                                    UBaseType_t bytesWaitingToBeRetrieved = 0;
                                    vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
                                    shouldWrite = bytesWaitingToBeRetrieved > 0;
                                }
                                break;

                                default:
                                    ESP_LOGE(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferWriting] - Failed to drain %u byte with error %d", takenFromBufferInBytes, err);
                                break;
                            }
                        } while (shouldWrite);

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
                        ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [%s] - New mode 'RingbufferPaused'", get_ringbuffer_mode_name(currentMode));
#endif
                        currentMode = RingbufferPaused;
                    }
                    break;

                    default:
                        ESP_LOGE(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [NotificationAudioPaused] Unhandled mode of operation %s (%d)", get_ringbuffer_mode_name(currentMode), currentMode);
                    break;
                }
            }
            break;

            default:
                ESP_LOGE(BT_I2S_OUTPUT_TAG, "i2s_task_handler() Unhandled notification type %s (%d)", get_i2s_task_notificationType(notification), notification);
            break;
        }

        // If we are waiting for enough audio data to be in the buffer, test now
        if (currentMode == RingbufferPrefetching) {
            size_t bytesWaitingToBeRetrieved = 0;
            vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
            int32_t remainToBuffer = MinimumPrefetchBufferSizeInBytes - bytesWaitingToBeRetrieved;
            float percentFetched = (100 * bytesWaitingToBeRetrieved) / MinimumPrefetchBufferSizeInBytes;
            ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferPrefetching] In buffer %u - Needs %ld - Buffered %f%%", bytesWaitingToBeRetrieved, remainToBuffer, percentFetched);
#endif

            if (bytesWaitingToBeRetrieved >= MinimumPrefetchBufferSizeInBytes) {
                currentMode = RingbufferWriting;

#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
                ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferPrefetching] Mode changed to 'RingbufferWriting'");
#endif
            } else {
                // Let audio data accumulate in buffer
                const TickType_t PrefetchDelayTimeInTicks = 5;
                vTaskDelay(PrefetchDelayTimeInTicks);
            }
        }

        // If we are writing to I2S, consume from ring buffer
        if (currentMode == RingbufferWriting) {
#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
            static uint64_t numberOfCalls = 0;
            numberOfCalls++;

            if (numberOfCalls % 100 == 0) {
                UBaseType_t bytesWaitingToBeRetrieved = 0;
                vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

                UBaseType_t freeBytes = RingBufferMaximumSizeInBytes - bytesWaitingToBeRetrieved;
                float percentOccupied = (100 * bytesWaitingToBeRetrieved) / RingBufferMaximumSizeInBytes;

                ESP_LOGI(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferWriting] Buffer waiting %u - Free %u - %f%% used", bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
            }
#endif
            const TickType_t WaitTimeInTicks = 10;
            size_t bytesTakenFromBuffer = 0;
            esp_err_t err = take_from_ringbuffer_and_write_to_i2s(s_bytes_to_take_from_ringbuffer, WaitTimeInTicks, &bytesTakenFromBuffer);
            if (err != ESP_OK) {
                switch (err) {
                    case ESP_ERR_TIMEOUT: {
#if CONFIG_HOLIDAYTREE_DETAILLED_I2S_DATA_PROCESSING_LOG
                        UBaseType_t bytesWaitingToBeRetrieved = 0;
                        vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
                        if (bytesWaitingToBeRetrieved > 0) {
                            ESP_LOGW(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferWriting] Ring buffer data read timeout - In buffer waiting %u", bytesWaitingToBeRetrieved);
                        }
#endif
                    }
                    break;

                    default:
                        ESP_LOGE(BT_I2S_OUTPUT_TAG, "i2s_task_handler() [RingbufferWriting] i2s_channel_write() failed with %d", err);
                    break;
                }
            }
        }
    }
}

static esp_err_t take_from_ringbuffer_and_write_to_i2s(size_t maxBytesToTakeFromBuffer, TickType_t readMaxWaitInTicks, size_t* pBytesTakenFromBuffer) {
    *pBytesTakenFromBuffer = 0;

    size_t sizeRetrievedFromRingBufferInBytes = 0;
    uint8_t *data = (uint8_t *) xRingbufferReceiveUpTo(s_i2s_ringbuffer, &sizeRetrievedFromRingBufferInBytes, readMaxWaitInTicks, maxBytesToTakeFromBuffer);
    if (data != NULL) {
        *pBytesTakenFromBuffer = sizeRetrievedFromRingBufferInBytes;

        size_t bytesWritten = 0;
        esp_err_t err = i2s_channel_write(s_i2s_tx_channel, (void*) data, sizeRetrievedFromRingBufferInBytes, &bytesWritten, portMAX_DELAY);
        vRingbufferReturnItem(s_i2s_ringbuffer, (void *) data);
        return err;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

static I2SWriterNotificationType_t accept_i2s_task_notification_with_delay(uint32_t delayMs) {
    uint32_t ulNotificationValue = 0UL;
    const UBaseType_t notificationIndex = I2STaskNotificationIndex;
    const TickType_t ticksToWait = delayMs != portMAX_DELAY ? pdMS_TO_TICKS(delayMs) : portMAX_DELAY;
    BaseType_t notificationWaitOutcome = xTaskNotifyWaitIndexed(notificationIndex, 0x0, 0x0, &ulNotificationValue, ticksToWait);
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

static esp_err_t notify_a2dp_audio_active() {
    return notify_i2s_task(NotificationAudioActive);
}

static esp_err_t notify_a2dp_audio_paused() {
    return notify_i2s_task(NotificationAudioPaused);
}

static esp_err_t notify_i2s_task(I2SWriterNotificationType_t notificationType) {
    const UBaseType_t notificationIndex = I2STaskNotificationIndex;
    ESP_LOGI(BT_I2S_RINGBUFFER_TAG, "Notifying I2S task -> Slot %d - Type '%s'", notificationIndex, get_i2s_task_notificationType(notificationType));
    const BaseType_t outcome = xTaskNotifyIndexed(s_i2s_task_handle, notificationIndex, notificationType, eSetValueWithOverwrite);
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
        case RingbufferNone:
            return "RingbufferNone";
        case RingbufferPaused:
            return "RingbufferPaused";
        case RingbufferPrefetching:
            return "RingbufferPrefetching";
        case RingbufferWriting:
            return "RingbufferWriting";
        default:
            return "N/A";
    }
}

static const char* get_i2s_task_notificationType(I2SWriterNotificationType_t notificationType) {
    switch (notificationType) {
        case NotificationNone:
            return "NotificationNone";
        case NotificationAudioActive:
            return "NotificationAudioActive";
        case NotificationAudioPaused:
            return "NotificationAudioPaused";
        default:
            return "N/A";
    }
}
