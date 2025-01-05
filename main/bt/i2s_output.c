// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

#include <esp_check.h>
#include <esp_log.h>

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
#include <esp_timer.h>
#endif

#include "bt/bt_avrc_volume.h"
#include "bt/i2s_output.h"


// I2S sound output log tags
static const char* BtI2sOutputTag = "i2s_output";
static const char* BtI2sRingbufferTag = "i2s_ringbuffer";


// Pins used to communicate with the I2S DAC / amplifier
static const gpio_num_t I2sDataPin = GPIO_NUM_25;
static const gpio_num_t I2sBckPin = GPIO_NUM_26;
static const gpio_num_t I2sLrckPin = GPIO_NUM_27;


// Number of audio bytes received from the A2DP callback (per call)
static const size_t A2DPBatchSizeInBytes = 4096; // 4K bytes

// Ring buffer size - Expressed as a multiple of the batch size (A2DPBatchSizeInBytes)
static const size_t RingBufferMaximumSizeInBytes = 8 * A2DPBatchSizeInBytes;

// Number of bytes to prefetch before sending data to the I2S output - Expressed as a multiple of the batch size (A2DPBatchSizeInBytes)
static const size_t MinimumPrefetchBufferSizeInBytes = 2 * A2DPBatchSizeInBytes;


// I2S task notification indices
static const UBaseType_t I2STaskNotificationIndex = 0;


// I2S task notification values
typedef enum {
    I2sWriterNotificationNone = -1,
    I2sWriterNotificationAudioActive = 1,
    I2sWriterNotificationAudioPaused = 2
} i2s_writer_notification_t;

// Ring buffer mode of operation
typedef enum {
    RingbufferNone = 0,
    RingbufferPaused = 1,       // Audio paused
    RingbufferPrefetching = 2,  // Buffering incoming audio data - I2S is waiting
    RingbufferWriting = 3       // Buffering incoming audio data - I2S output writing to DMA
} ringbuffer_mode_t;


static i2s_chan_handle_t s_i2s_tx_channel = NULL;
static TaskHandle_t s_i2s_task_handle = NULL;
static RingbufHandle_t s_i2s_ringbuffer = NULL;

static uint8_t s_bytes_per_sample_per_channel = 0;
static size_t s_bytes_to_take_from_ringbuffer = 0;


static esp_err_t create_i2s_channel();
static esp_err_t delete_i2s_channel();

static esp_err_t start_i2s_output_task();
static esp_err_t stop_i2s_output_task();

static void i2s_task_handler(void* arg);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
static void log_ringbuffer_incoming_stats(uint32_t size);
static void log_ringbuffer_write_stats(uint64_t startEspTime, uint64_t endEspTime);
#endif

static esp_err_t take_from_ringbuffer_and_write_to_i2s(size_t maxBytesToTakeFromBuffer, TickType_t readMaxWaitInTicks, size_t* pBytesTakenFromBuffer);
static void apply_volume(void* data, size_t len, uint8_t bytePerSample);

static i2s_writer_notification_t accept_i2s_task_notification_with_delay(TickType_t delayTicks);

static esp_err_t notify_a2dp_audio_active();
static esp_err_t notify_a2dp_audio_paused();
static esp_err_t notify_i2s_task(i2s_writer_notification_t notificationType);

static void get_dma_buffer_size_and_buffer_count_for_data_buffer_size(size_t batchSize, i2s_data_bit_width_t sampleBits, uint8_t channelCount, uint32_t* pDmaDescNum, uint32_t* pDmaFrameNum, size_t* pBytesToTakeFromRingBuffer);
static const char* get_ringbuffer_mode_name(ringbuffer_mode_t ringbufferMode);
static const char* get_i2s_task_notificationType(i2s_writer_notification_t notificationType);


esp_err_t create_i2s_output() {
    return create_i2s_channel();
}

esp_err_t start_i2s_output() {
    return start_i2s_output_task();
}

esp_err_t delete_i2s_output() {
#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
    ESP_LOGI(BtI2sOutputTag, "delete_i2s_output() - Stopping I2S task");
#endif

    esp_err_t err = stop_i2s_output_task();
    if (err != ESP_OK) {
        ESP_LOGW(BtI2sOutputTag, "stop_i2s_output_task() failed while shutting down I2S output task (%d)", err);
    }

#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
    ESP_LOGI(BtI2sOutputTag, "delete_i2s_output() - Deleting I2S channel");
#endif

    err = delete_i2s_channel();
    if (err != ESP_OK) {
        ESP_LOGW(BtI2sOutputTag, "delete_i2s_channel() failed while shutting down I2S channel (%d)", err);
    }

    return err;
}

esp_err_t configure_i2s_output(uint32_t sampleRate, i2s_data_bit_width_t dataWidth, i2s_slot_mode_t slotMode) {
    // Disable the transmission channel so it can be reconfigured
    ESP_RETURN_ON_ERROR(i2s_channel_disable(s_i2s_tx_channel), BtI2sOutputTag, "i2s_channel_disable() failed");

    // Re-configure clock 
    i2s_std_clk_config_t clkCfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
    ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_clock(s_i2s_tx_channel, &clkCfg), BtI2sOutputTag, "i2s_channel_reconfig_std_clock(%lu) failed", sampleRate);

    // Re-configure slot
    i2s_std_slot_config_t slotCfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(dataWidth, slotMode);
    ESP_RETURN_ON_ERROR(i2s_channel_reconfig_std_slot(s_i2s_tx_channel, &slotCfg), BtI2sOutputTag, "i2s_channel_reconfig_std_slot(%d) failed", slotMode);

    // Enable the channel
    ESP_RETURN_ON_ERROR(i2s_channel_enable(s_i2s_tx_channel), BtI2sOutputTag, "i2s_channel_enable");

    // Cache per channel data width in byte - We currently only support only SBC which is 16 bits per sample per channel
    s_bytes_per_sample_per_channel = dataWidth / 8;

    return ESP_OK;
}

static esp_err_t create_i2s_channel() {
    // DMA configuration - It is fixed at channel creation and cannot be changed later unless the channel is deleted
    uint32_t dmaDescNum = 0;
    uint32_t dmaFrameNum = 0;
    get_dma_buffer_size_and_buffer_count_for_data_buffer_size(A2DPBatchSizeInBytes, I2S_DATA_BIT_WIDTH_16BIT, 2, &dmaDescNum, &dmaFrameNum, &s_bytes_to_take_from_ringbuffer);

    // Configure I2S channel - See I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER)
    i2s_chan_config_t channelCfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = dmaDescNum,     // 6 Buffers = 6 DMA descriptors
        .dma_frame_num = dmaFrameNum,   // 240 Frames (aka samples)
        .auto_clear = true,             // Clear DMA TX buffer to send 0 automatically if no data to send - Otherwise the last data is sent creating an effect of "repeating sample"
        .intr_priority = 0              // Priority level - When 0, the driver allocates an interrupt with "low" priority (1,2,3)
    };

    // Standard configuration for I2S - Assume 44.1kHz - Frequency, sample size and number of channels can be changed without deleting the channel
    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2sBckPin,
            .ws = I2sLrckPin,
            .dout = I2sDataPin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_ERROR(i2s_new_channel(&channelCfg, &s_i2s_tx_channel, NULL), cleanup, BtI2sOutputTag, "i2s_new_channel() failed");
    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_i2s_tx_channel, &stdCfg), cleanup, BtI2sOutputTag, "i2s_channel_init_std_mode() failed");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(s_i2s_tx_channel), cleanup, BtI2sOutputTag, "i2s_channel_enable() failed");

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
            ESP_LOGW(BtI2sOutputTag, "i2s_channel_disable() failed while shutting down I2S (%d)", err);
        }

        if ((err = i2s_del_channel(s_i2s_tx_channel)) != ESP_OK) {
            ESP_LOGW(BtI2sOutputTag, "i2s_del_channel() failed while shutting down I2S (%d)", err);
        }

        s_i2s_tx_channel = NULL;
    }

    return err;
}

static esp_err_t start_i2s_output_task() {
#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
    ESP_LOGI(BtI2sOutputTag, "Starting I2S output");
#endif

    esp_err_t err = ESP_OK;

    // Create ring buffer
    s_i2s_ringbuffer = xRingbufferCreate(RingBufferMaximumSizeInBytes, RINGBUF_TYPE_BYTEBUF);
    if (s_i2s_ringbuffer == NULL) {
        ESP_LOGE(BtI2sOutputTag, "start_i2s_output_task() - xRingbufferCreate() failed");
        err = ESP_FAIL;
        goto cleanup;
    }

    // Create output task - It runs on the core not assigned to BlueDroid
    const BaseType_t appCoreId = CONFIG_BT_BLUEDROID_PINNED_TO_CORE == PRO_CPU_NUM ? APP_CPU_NUM : PRO_CPU_NUM;
    BaseType_t taskCreated = xTaskCreatePinnedToCore(i2s_task_handler, "ht-BT-I2S", 2048, NULL, configMAX_PRIORITIES - 3, &s_i2s_task_handle, appCoreId);
    err = taskCreated == pdPASS ? ESP_OK : ESP_FAIL;
    if (err != ESP_OK) {
        ESP_LOGE(BtI2sOutputTag, "start_i2s_output_task() - xTaskCreate() failed");
    }

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
            ESP_LOGE(BtI2sRingbufferTag, "set_i2s_output_audio_state() - Unable to notify I2S task of audio state %d", audioState);
            return ESP_FAIL;
    }
}

uint32_t write_to_i2s_output(const uint8_t* data, uint32_t size) {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
    log_ringbuffer_incoming_stats(size);
    uint64_t startEspTime = esp_timer_get_time();
#endif

    // --------------------------------------------------------------------------------------------
    // xRingbufferSend() on ESP32 was measured as follows (for a buffer of 4096 bytes of data):
    //  - Average time:   26 us
    //  - Minimum time:   23 us
    //  - Maximum time: 8363 us
    //
    // With the default FReeRTOS configuration (configTICK_RATE_HZ = 100Hz):
    //  - Anything below1 000 us is less than 1 FreeRTOS tick
    //
    // We choose `WriteWaitTimeInTicks` below as 10 times the maximum xRingbufferSend() time
    // --------------------------------------------------------------------------------------------
    const TickType_t WriteWaitTimeInTicks = pdMS_TO_TICKS(10 * 1);
    BaseType_t ringBufferSendOutcome = xRingbufferSend(s_i2s_ringbuffer, (void *)data, size, WriteWaitTimeInTicks);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        uint64_t endEspTime = esp_timer_get_time();
#endif

    if (ringBufferSendOutcome == pdTRUE) {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        log_ringbuffer_write_stats(startEspTime, endEspTime);
#endif
        return size;
    }
    else {
        ESP_LOGE(BtI2sRingbufferTag, "write_to_i2s_output() - Timed out trying to write to ring buffer or ring buffer overflow - Dropped %lu bytes", size);
        return 0;
    }
}


#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG

static void log_ringbuffer_incoming_stats(uint32_t size) {
    static uint64_t numberOfCalls = 0;

    numberOfCalls++;

    if (numberOfCalls % 100 == 0) {
        UBaseType_t bytesWaitingToBeRetrieved = 0;
        vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

        UBaseType_t freeBytes = RingBufferMaximumSizeInBytes - bytesWaitingToBeRetrieved;
        float percentOccupied = (100 * bytesWaitingToBeRetrieved) / RingBufferMaximumSizeInBytes;

        ESP_LOGI(BtI2sOutputTag, "write_to_i2s_output() Writing %lu bytes | Ringbuffer stats - Waiting: %u bytes - Free: %u bytes - %f%% used", size, bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
    }
}

static void log_ringbuffer_write_stats(uint64_t startEspTime, uint64_t endEspTime) {
    // Total number of calls
    static uint64_t numberOfCalls = 0;
    numberOfCalls++;

    // Current call time in us
    uint64_t thisCallTime = endEspTime - startEspTime;

    // Total call duration across all calls
    static uint64_t totallCallTime = 0;
    totallCallTime += thisCallTime;

    // Minimum and maximum
    static uint64_t minTimePerCall = UINT64_MAX;
    static uint64_t maxTimePerCall = 0;

    minTimePerCall = minTimePerCall > thisCallTime ? thisCallTime : minTimePerCall;
    maxTimePerCall = maxTimePerCall < thisCallTime ? thisCallTime : maxTimePerCall;

    if (numberOfCalls % 100 == 0) {
        // Current call time in FreeRTOS ticks and average time per call
        uint32_t thisCallTimeTicks = pdMS_TO_TICKS(thisCallTime / 1000);
        uint64_t averageTimePerCall = totallCallTime / numberOfCalls;

        ESP_LOGI(BtI2sOutputTag, "write_to_i2s_output() xRingBufferSend() time -> This call: %llu us (%lu Ticks) - Average: %llu us - Min: %llu us - Max: %llu us", thisCallTime, thisCallTimeTicks, averageTimePerCall, minTimePerCall, maxTimePerCall);
    }
}

#endif


static void i2s_task_handler(void* arg) {
    ringbuffer_mode_t currentMode = RingbufferNone;

    for (;;) {
        TickType_t notificationDelay = (currentMode == RingbufferWriting) || (currentMode == RingbufferPrefetching) ? 2 : portMAX_DELAY;
        i2s_writer_notification_t notification = accept_i2s_task_notification_with_delay(notificationDelay);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
    if (notification != I2sWriterNotificationNone) {
        ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [%s] - Received notification '%s'", get_ringbuffer_mode_name(currentMode), get_i2s_task_notificationType(notification));
    }
#endif

        switch (notification) {
            case I2sWriterNotificationNone:
            break;

            case I2sWriterNotificationAudioActive:{
                switch (currentMode) {
                    case RingbufferNone:
                    case RingbufferPaused: {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                        ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [%s] - New mode 'RingbufferPrefetching'", get_ringbuffer_mode_name(currentMode));
#endif
                        currentMode = RingbufferPrefetching;
                    }
                    break;

                    case RingbufferPrefetching:
                    case RingbufferWriting:
                    break;

                    default: 
                        ESP_LOGE(BtI2sOutputTag, "i2s_task_handler() [I2sWriterNotificationAudioActive] Unhandled mode of operation %s (%d)", get_ringbuffer_mode_name(currentMode), currentMode);
                    break;
                }
            }
            break;

            case I2sWriterNotificationAudioPaused: {
                switch (currentMode) {
                    case RingbufferNone:
                    case RingbufferPaused:
                        currentMode = RingbufferPaused;
                    break;

                    case RingbufferPrefetching:
                    case RingbufferWriting: {
#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
                        ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [%s] - Draining buffer before switching to 'RingbufferPaused' mode", get_ringbuffer_mode_name(currentMode));
#endif
                        bool shouldWrite = true;
                        do {
                            const TickType_t DrainWaitTimeInTicks = 10;
                            size_t takenFromBufferInBytes = 0;
                            esp_err_t err = take_from_ringbuffer_and_write_to_i2s(s_bytes_to_take_from_ringbuffer, DrainWaitTimeInTicks, &takenFromBufferInBytes);
                            switch (err) {
                                case ESP_OK:
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                                    ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [RingbufferWriting] - Drained %u bytes",  takenFromBufferInBytes);
#endif
                                break;

                                case ESP_ERR_TIMEOUT: {
                                    UBaseType_t bytesWaitingToBeRetrieved = 0;
                                    vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
                                    shouldWrite = bytesWaitingToBeRetrieved > 0;
                                }
                                break;

                                default:
                                    ESP_LOGE(BtI2sOutputTag, "i2s_task_handler() [RingbufferWriting] - Failed to drain %u byte with error %d", takenFromBufferInBytes, err);
                                break;
                            }
                        } while (shouldWrite);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                        ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [%s] - New mode 'RingbufferPaused'", get_ringbuffer_mode_name(currentMode));
#endif
                        currentMode = RingbufferPaused;
                    }
                    break;

                    default:
                        ESP_LOGE(BtI2sOutputTag, "i2s_task_handler() [I2sWriterNotificationAudioPaused] Unhandled mode of operation %s (%d)", get_ringbuffer_mode_name(currentMode), currentMode);
                    break;
                }
            }
            break;

            default:
                ESP_LOGE(BtI2sOutputTag, "i2s_task_handler() Unhandled notification type %s (%d)", get_i2s_task_notificationType(notification), notification);
            break;
        }

        // If we are waiting for enough audio data to be in the buffer, test now
        if (currentMode == RingbufferPrefetching) {
            size_t bytesWaitingToBeRetrieved = 0;
            vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
            int32_t remainToBuffer = MinimumPrefetchBufferSizeInBytes - bytesWaitingToBeRetrieved;
            float percentFetched = (100 * bytesWaitingToBeRetrieved) / MinimumPrefetchBufferSizeInBytes;
            ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [RingbufferPrefetching] In buffer %u - Needs %ld - Buffered %f%%", bytesWaitingToBeRetrieved, remainToBuffer, percentFetched);
#endif

            if (bytesWaitingToBeRetrieved >= MinimumPrefetchBufferSizeInBytes) {
                currentMode = RingbufferWriting;

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [RingbufferPrefetching] Mode changed to 'RingbufferWriting'");
#endif
            } else {
                // Let audio data accumulate in buffer
                const TickType_t PrefetchDelayTimeInTicks = 5;
                vTaskDelay(PrefetchDelayTimeInTicks);
            }
        }

        // If we are writing to I2S, consume from ring buffer
        if (currentMode == RingbufferWriting) {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
            static uint64_t numberOfCalls = 0;
            numberOfCalls++;

            if (numberOfCalls % 100 == 0) {
                UBaseType_t bytesWaitingToBeRetrieved = 0;
                vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

                UBaseType_t freeBytes = RingBufferMaximumSizeInBytes - bytesWaitingToBeRetrieved;
                float percentOccupied = (100 * bytesWaitingToBeRetrieved) / RingBufferMaximumSizeInBytes;

                ESP_LOGI(BtI2sOutputTag, "i2s_task_handler() [RingbufferWriting] Buffer waiting %u - Free %u - %f%% used", bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
            }
#endif
            const TickType_t WaitTimeInTicks = 10;
            size_t bytesTakenFromBuffer = 0;
            esp_err_t err = take_from_ringbuffer_and_write_to_i2s(s_bytes_to_take_from_ringbuffer, WaitTimeInTicks, &bytesTakenFromBuffer);
            if (err != ESP_OK) {
                switch (err) {
                    case ESP_ERR_TIMEOUT: {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                        UBaseType_t bytesWaitingToBeRetrieved = 0;
                        vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
                        if (bytesWaitingToBeRetrieved > 0) {
                            ESP_LOGW(BtI2sOutputTag, "i2s_task_handler() [RingbufferWriting] Ring buffer data read timeout - In buffer waiting %u", bytesWaitingToBeRetrieved);
                        }
#endif
                    }
                    break;

                    default:
                        ESP_LOGE(BtI2sOutputTag, "i2s_task_handler() [RingbufferWriting] i2s_channel_write() failed with %d", err);
                    break;
                }
            }
        }
    }
}

static esp_err_t take_from_ringbuffer_and_write_to_i2s(size_t maxBytesToTakeFromBuffer, TickType_t readMaxWaitInTicks, size_t* pBytesTakenFromBuffer) {
    *pBytesTakenFromBuffer = 0;

    // Retrieve the number of available bytes - We would like to read a multiple of samples so we can apply software volume in a mneaingful way
    UBaseType_t bytesWaitingToBeRetrieved = 0;
    vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
    size_t maxToRetrieveUnaligned = bytesWaitingToBeRetrieved > maxBytesToTakeFromBuffer ? maxBytesToTakeFromBuffer : bytesWaitingToBeRetrieved;

    // Align to boundaries of the number of bytes per channel - In practice, this is 2 bytes
    size_t bytesToTake = (maxToRetrieveUnaligned >> (s_bytes_per_sample_per_channel - 1)) << (s_bytes_per_sample_per_channel - 1);
    if (bytesToTake > 0) {
        size_t sizeRetrievedFromRingBufferInBytes = 0;
        void* data = xRingbufferReceiveUpTo(s_i2s_ringbuffer, &sizeRetrievedFromRingBufferInBytes, readMaxWaitInTicks, bytesToTake);
        if (data != NULL) {
            *pBytesTakenFromBuffer = sizeRetrievedFromRingBufferInBytes;

            apply_volume(data, sizeRetrievedFromRingBufferInBytes, s_bytes_per_sample_per_channel);

            size_t bytesWritten = 0;
            esp_err_t err = i2s_channel_write(s_i2s_tx_channel, (void*) data, sizeRetrievedFromRingBufferInBytes, &bytesWritten, portMAX_DELAY);
            vRingbufferReturnItem(s_i2s_ringbuffer, (void *) data);
            return err;
        } else {
            return ESP_ERR_TIMEOUT;
        }
    }

    return ESP_ERR_TIMEOUT;
}

static void apply_volume(void* data, size_t len, uint8_t bytePerSample) {
    const float volumeFactor = get_volume_factor();

    // TODO: We currently only support I2S_DATA_BIT_WIDTH_16BIT or 2 bytes per channel
    uint16_t* incomingData = (uint16_t*) data;
    for (size_t dataIndex = 0; dataIndex < len / bytePerSample; dataIndex++) {
        int16_t pcmData = incomingData[dataIndex];
        int32_t pcmDataWithVolume = (int32_t) roundf(pcmData * volumeFactor);
        uint16_t truncatedPcmDataWithVolume = pcmDataWithVolume;
        incomingData[dataIndex] = truncatedPcmDataWithVolume;
    }
}

static i2s_writer_notification_t accept_i2s_task_notification_with_delay(TickType_t delayTicks) {
    uint32_t ulNotificationValue = 0UL;
    const UBaseType_t notificationIndex = I2STaskNotificationIndex;
    BaseType_t notificationWaitOutcome = xTaskNotifyWaitIndexed(notificationIndex, 0x0, 0x0, &ulNotificationValue, delayTicks);
    ESP_LOGD(BtI2sRingbufferTag, "accept_i2s_task_notification_with_delay() - xTaskNotifyWaitIndexed() [Returned: %d] [Value: %lu] [Timeout: %lu ticks]", notificationWaitOutcome, ulNotificationValue, delayTicks);
    switch (notificationWaitOutcome) {
        case pdTRUE:
            // Notification received
            return ulNotificationValue;
        case pdFALSE:
            // Timeout - No notification was received
            return I2sWriterNotificationNone;
        default:
            // Unknown notification - Log and ignore the unknown message
            ESP_LOGE(BtI2sRingbufferTag, "accept_i2s_task_notification_with_delay() - xTaskNotifyWaitIndexed() received unknown notification (%lu)", ulNotificationValue);
            return I2sWriterNotificationNone;
    }
}

static esp_err_t notify_a2dp_audio_active() {
    return notify_i2s_task(I2sWriterNotificationAudioActive);
}

static esp_err_t notify_a2dp_audio_paused() {
    return notify_i2s_task(I2sWriterNotificationAudioPaused);
}

static esp_err_t notify_i2s_task(i2s_writer_notification_t notificationType) {
    const UBaseType_t notificationIndex = I2STaskNotificationIndex;

#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
    ESP_LOGI(BtI2sRingbufferTag, "Notifying I2S task -> Slot %d - Type '%s'", notificationIndex, get_i2s_task_notificationType(notificationType));
#endif

    const BaseType_t outcome = xTaskNotifyIndexed(s_i2s_task_handle, notificationIndex, notificationType, eSetValueWithOverwrite);
    return outcome == pdPASS ? ESP_OK : ESP_FAIL;
}

static void get_dma_buffer_size_and_buffer_count_for_data_buffer_size(size_t batchSize, i2s_data_bit_width_t sampleBits, uint8_t channelCount, uint32_t* pDmaDescNum, uint32_t* pDmaFrameNum, size_t* pBytesToTakeFromRingBuffer) {
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
        *pDmaFrameNum = numberOfSamplesInBatch;
        *pDmaDescNum = 4;

        // We will take up to one DMA buffer worth of bytes from the ringbuffer per I2S write
        *pBytesToTakeFromRingBuffer = numberOfSamplesInBatch * bytesPerSample;
    } else {
        // All samples in one batch do not fit in one DMA buffer - We request 2 times the number of buffers we need
        *pDmaFrameNum = maxSamplesCountAllowedForDMABufferSize;
        *pDmaDescNum = 2 * ((numberOfSamplesInBatch / maxSamplesCountAllowedForDMABufferSize) + 1);

        // We will take up to one DMA buffer worth of bytes from the ringbuffer per I2S write
        *pBytesToTakeFromRingBuffer = FrameNumMaxInBytes;
    }
    
#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
    ESP_LOGI(BtI2sOutputTag, "DMA dma_frame_num: %lu - DMA dma_desc_num: %lu - I2S write size: %u | Batch size %u, Sample size %d, Channels %d", *pDmaFrameNum, *pDmaDescNum, *pBytesToTakeFromRingBuffer, batchSize, sampleBits, channelCount);
#endif
}

static const char* get_ringbuffer_mode_name(ringbuffer_mode_t ringbufferMode) {
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

static const char* get_i2s_task_notificationType(i2s_writer_notification_t notificationType) {
    switch (notificationType) {
        case I2sWriterNotificationNone:
            return "I2sWriterNotificationNone";
        case I2sWriterNotificationAudioActive:
            return "I2sWriterNotificationAudioActive";
        case I2sWriterNotificationAudioPaused:
            return "I2sWriterNotificationAudioPaused";
        default:
            return "N/A";
    }
}
