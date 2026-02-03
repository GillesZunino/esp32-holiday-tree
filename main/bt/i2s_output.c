// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <stdatomic.h>

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
static const size_t MinimumPrefetchBufferSizeInBytes = 1 * A2DPBatchSizeInBytes;


// I2S task notification index and value
static const UBaseType_t I2STaskNotificationIndex = 0;
static const uint32_t I2STaskNotificationValue = ULONG_MAX;


// A2DP Audio state
typedef enum {
    A2DPAudioStateNone = -1,
    A2DPAudioStateActive = 1,
    A2DPAudioStatePaused = 2
} a2dp_audio_state_t;

// Ring buffer mode of operation
typedef enum {
    RingbufferNone = 0,
    RingbufferPrefetching = 1,  // Buffering incoming audio data - I2S is waiting
    RingbufferWriting = 2,      // Buffering incoming audio data - Data sent to I2S output via DMA 
} ringbuffer_mode_t;


static i2s_chan_handle_t s_i2s_tx_channel = NULL;
static TaskHandle_t s_i2s_task_handle = NULL;
static RingbufHandle_t s_i2s_ringbuffer = NULL;

static uint8_t s_bytes_per_sample_per_channel = 0;
static size_t s_bytes_to_take_from_ringbuffer = 0;

static uint8_t* s_i2s_audio_processing_buffer = NULL;

static volatile atomic_uint_fast8_t s_atomic_current_audio_state = A2DPAudioStateNone;


static esp_err_t create_i2s_channel();
static esp_err_t delete_i2s_channel();

static esp_err_t start_i2s_output_task();
static esp_err_t stop_i2s_output_task();

static void i2s_task_handler(void* arg);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
static void log_ringbuffer_incoming_stats(uint32_t size);
static void log_ringbuffer_outgoing_stats(BaseType_t bytesWaitingToBeRetrieved, ringbuffer_mode_t ringbufferMode);
static void log_ringbuffer_operation_stats(uint64_t startEspTime, uint64_t endEspTime, const char* const operationName);
#endif

static esp_err_t take_from_ringbuffer_and_write_to_i2s(size_t maxBytesToTakeFromBuffer);
static void apply_volume(void* data, size_t len, uint8_t bytePerSample);
static void drain_ringbuffer();

static esp_err_t notify_a2dp_audio_active();
static esp_err_t notify_a2dp_audio_paused();
static esp_err_t notify_i2s_task(a2dp_audio_state_t audioState);

static void get_dma_buffer_size_and_buffer_count_for_data_buffer_size(size_t batchSize, i2s_data_bit_width_t sampleBits, uint8_t channelCount, uint32_t* pDmaDescNum, uint32_t* pDmaFrameNum, size_t* pBytesToTakeFromRingBuffer);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
static const char* get_ringbuffer_mode_name(ringbuffer_mode_t ringbufferMode);
#endif


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

    // Cache per channel data width in byte - We currently only support SBC which is 16 bits per sample per channel
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
    ESP_LOGI(BtI2sOutputTag, "Starting I2S output task");
#endif

    esp_err_t err = ESP_OK;

    // No known A2DP audio state
    atomic_store(&s_atomic_current_audio_state, A2DPAudioStateNone);

    // Allocate audio processing buffer
    s_i2s_audio_processing_buffer = (uint8_t*)heap_caps_calloc(1, s_bytes_to_take_from_ringbuffer, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (s_i2s_audio_processing_buffer == NULL) {
        ESP_LOGE(BtI2sOutputTag, "start_i2s_output_task() - heap_caps_calloc() failed");
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    // Create ring buffer
    s_i2s_ringbuffer = xRingbufferCreate(RingBufferMaximumSizeInBytes, RINGBUF_TYPE_BYTEBUF);
    if (s_i2s_ringbuffer == NULL) {
        ESP_LOGE(BtI2sOutputTag, "start_i2s_output_task() - xRingbufferCreate() failed");
        err = ESP_FAIL;
        goto cleanup;
    }

    // Create output task - It runs on the core not assigned to BlueDroid
    const BaseType_t appCoreId = CONFIG_BT_BLUEDROID_PINNED_TO_CORE == PRO_CPU_NUM ? APP_CPU_NUM : PRO_CPU_NUM;
    const uint32_t StackSize = ( CONFIG_HOLIDAYTREE_I2S_TASK_STACK_SIZE );
    BaseType_t taskCreated = xTaskCreatePinnedToCore(i2s_task_handler, "ht-BT-I2S", StackSize, NULL, configMAX_PRIORITIES - 3, &s_i2s_task_handle, appCoreId);
    err = taskCreated == pdPASS ? ESP_OK : ESP_FAIL;
    if (err != ESP_OK) {
        ESP_LOGE(BtI2sOutputTag, "start_i2s_output_task() - xTaskCreate() failed");
    }

cleanup:
    if (err != ESP_OK) {
        stop_i2s_output_task();
    }

    return err;
}

static esp_err_t stop_i2s_output_task() {
#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
    ESP_LOGI(BtI2sOutputTag, "Stopping I2S output task");
#endif

    if (s_i2s_task_handle != NULL) {
        vTaskDelete(s_i2s_task_handle);
        s_i2s_task_handle = NULL;

        atomic_store(&s_atomic_current_audio_state, A2DPAudioStateNone);
    }
    if (s_i2s_ringbuffer != NULL) {
        vRingbufferDelete(s_i2s_ringbuffer);
        s_i2s_ringbuffer = NULL;
    }
    if (s_i2s_audio_processing_buffer != NULL) {
        heap_caps_free(s_i2s_audio_processing_buffer);
        s_i2s_audio_processing_buffer = NULL;
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
    // With the default FreeRTOS configuration (configTICK_RATE_HZ = 100Hz):
    //  - Anything below 1000 us is less than 1 FreeRTOS tick
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
        log_ringbuffer_operation_stats(startEspTime, endEspTime, "xRingBufferSend()");
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

        ESP_LOGI(BtI2sOutputTag, "[Ringbuffer] Writing %lu | Stats - Waiting: %u bytes - Free: %u bytes - Usage: %f%%", size, bytesWaitingToBeRetrieved, freeBytes, percentOccupied);
    }
}

static void log_ringbuffer_outgoing_stats(BaseType_t bytesWaitingToBeRetrieved, ringbuffer_mode_t ringbufferMode) {
    static uint64_t numberOfCalls = 0;
    numberOfCalls++;

    if (numberOfCalls % 100 == 0) {
        int32_t remainToBuffer = MinimumPrefetchBufferSizeInBytes - bytesWaitingToBeRetrieved;
        float percentFetched = (100 * bytesWaitingToBeRetrieved) / MinimumPrefetchBufferSizeInBytes;
        UBaseType_t freeBytes = RingBufferMaximumSizeInBytes - bytesWaitingToBeRetrieved;
        float percentOccupied = (100 * bytesWaitingToBeRetrieved) / RingBufferMaximumSizeInBytes;
        ESP_LOGI(BtI2sOutputTag, "[Ringbuffer] [%s] In buffer %u - Needs %ld - Buffered %f%% | Buffer Free %u - Occupied %f%%", get_ringbuffer_mode_name(ringbufferMode), bytesWaitingToBeRetrieved, remainToBuffer, percentFetched, freeBytes, percentOccupied);
    }
}

static void log_ringbuffer_operation_stats(uint64_t startEspTime, uint64_t endEspTime, const char* const operationName) {
    static uint64_t numberOfCalls = 0;
    numberOfCalls++;

    // Current call time in us
    uint64_t thisCallTime = endEspTime - startEspTime;

    // Total call duration across all calls
    static uint64_t totalCallTime = 0;
    totalCallTime += thisCallTime;

    // Minimum and maximum
    static uint64_t minTimePerCall = UINT64_MAX;
    static uint64_t maxTimePerCall = 0;

    minTimePerCall = minTimePerCall > thisCallTime ? thisCallTime : minTimePerCall;
    maxTimePerCall = maxTimePerCall < thisCallTime ? thisCallTime : maxTimePerCall;

    if (numberOfCalls % 100 == 0) {
        // Current call time in FreeRTOS ticks and average time per call
        uint32_t thisCallTimeTicks = pdMS_TO_TICKS(thisCallTime / 1000);
        uint64_t averageTimePerCall = totalCallTime / numberOfCalls;

        ESP_LOGI(BtI2sOutputTag, "[Ringbuffer] %s | Stats - This call: %llu us (%lu Ticks) - Average: %llu us - Min: %llu us - Max: %llu us", operationName, thisCallTime, thisCallTimeTicks, averageTimePerCall, minTimePerCall, maxTimePerCall);
    }
}

#endif


static void i2s_task_handler(void* arg) {
    for (;;) {
        // Wait for an A2DP "Audio Start" notification - The task is notified only when A2DP audio state changes from 'Paused' to 'Active'
        uint32_t ulNotificationValue = 0UL;

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        BaseType_t notificationWaitOutcome =
#endif
        xTaskNotifyWaitIndexed(I2STaskNotificationIndex, 0x0, ULONG_MAX, &ulNotificationValue, portMAX_DELAY);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        ESP_LOGI(BtI2sRingbufferTag, "i2s_task_handler() - xTaskNotifyWaitIndexed() [Returned: %d] [Value: %lu]", notificationWaitOutcome, ulNotificationValue);
#endif

        // Unknown ring buffer mode when A2DP audio becomes active
        ringbuffer_mode_t ringbufferMode = RingbufferNone;

        // Unknown A2DP audio state
        a2dp_audio_state_t audioState = A2DPAudioStateNone;

        do {
            // Did we prefetch enough audio data to start writing to I2S?
            audioState = atomic_load(&s_atomic_current_audio_state);
            if (audioState == A2DPAudioStateActive) {
                UBaseType_t bytesWaitingToBeRetrieved = 0;
                vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
                ringbufferMode = bytesWaitingToBeRetrieved >= MinimumPrefetchBufferSizeInBytes ? RingbufferWriting : RingbufferPrefetching;

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                log_ringbuffer_outgoing_stats(bytesWaitingToBeRetrieved, ringbufferMode);
#endif
            }

            // Are we ready to write audio data to I2S?
            audioState = atomic_load(&s_atomic_current_audio_state);
            if (audioState == A2DPAudioStateActive) {
                if (ringbufferMode == RingbufferWriting) {
                    esp_err_t err = take_from_ringbuffer_and_write_to_i2s(s_bytes_to_take_from_ringbuffer);
                    if (err != ESP_OK) {
                        ESP_LOGW(BtI2sRingbufferTag, "i2s_task_handler() - take_from_ringbuffer_and_write_to_i2s() failed (%d) - [s_bytes_to_take_from_ringbuffer: %u]", err, s_bytes_to_take_from_ringbuffer);
                    }
                } 
            }
            
            // Are we pausing audio ?
            audioState = atomic_load(&s_atomic_current_audio_state);
            if (audioState == A2DPAudioStatePaused) {
                drain_ringbuffer();
            }

            // When prefetching is necessary, wait for a little for the ring buffer to fill up
            audioState = atomic_load(&s_atomic_current_audio_state);
            if (audioState == A2DPAudioStateActive) {
                if (ringbufferMode == RingbufferPrefetching) {
                    const TickType_t PrefetchDelayTimeInTicks = 1;
                    vTaskDelay(PrefetchDelayTimeInTicks);
                }
            }
        } while (audioState == A2DPAudioStateActive);
    }
}

static esp_err_t take_from_ringbuffer_and_write_to_i2s(size_t maxBytesToTakeFromBuffer) {
    esp_err_t err = ESP_ERR_INVALID_SIZE;

    // Retrieve the number of available bytes - We would like to read a multiple of samples so we can apply software volume in a meaningful way
    UBaseType_t bytesWaitingToBeRetrieved = 0;
    vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);
    size_t maxToRetrieveUnaligned = bytesWaitingToBeRetrieved > maxBytesToTakeFromBuffer ? maxBytesToTakeFromBuffer : bytesWaitingToBeRetrieved;

    // Align to boundaries of the number of bytes per channel - In practice, this is 2 bytes
    size_t bytesToTake = (maxToRetrieveUnaligned >> (s_bytes_per_sample_per_channel - 1)) << (s_bytes_per_sample_per_channel - 1);
    if (bytesToTake > 0) {
        size_t sizeRetrievedFromRingBufferInBytes = 0;

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        uint64_t ringbufferReceiveStartEspTime = esp_timer_get_time();
#endif
        // ------------------------------------------------------------------------------------------------
        // xRingbufferReceiveUpTo() on ESP32 was measured as follows (for a buffer of 4096 bytes of data):
        //  - Average time:  3 us
        //  - Minimum time:  3 us
        //  - Maximum time: 18 us
        //
        // With the default FreeRTOS configuration (configTICK_RATE_HZ = 100Hz):
        //  - Anything below 1000 us is less than 1 FreeRTOS tick
        //
        // We choose `ReadWaitTimeInTicks` below as 10 times the maximum xRingbufferReceiveUpTo() time
        // ------------------------------------------------------------------------------------------------
        const TickType_t ReadWaitTimeInTicks = pdMS_TO_TICKS(10 * 1);
        void* data = xRingbufferReceiveUpTo(s_i2s_ringbuffer, &sizeRetrievedFromRingBufferInBytes, ReadWaitTimeInTicks, bytesToTake);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        uint64_t ringbufferReceiveEndEspTime = esp_timer_get_time();
#endif
        if (data != NULL) {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
            log_ringbuffer_operation_stats(ringbufferReceiveStartEspTime, ringbufferReceiveEndEspTime, "xRingbufferReceiveUpTo()");
#endif
            // Copy the received data to our processing buffer and return the taken bytes to the ringbuffer
            memcpy(s_i2s_audio_processing_buffer, data, sizeRetrievedFromRingBufferInBytes);
            vRingbufferReturnItem(s_i2s_ringbuffer, data);

            // ------------------------------------------------------------------------------------------------
            // xRingbufferReceiveUpTo() needs to be called twice when the ring buffer wraps around
            // We detect this condition and append the result of the second call to the data retrieved
            //
            // This is necessary because our audio processing expects a finite number of samples to process
            // Failure to do so creates audibles pops and clicks in the audio stream
            // ------------------------------------------------------------------------------------------------
            err = sizeRetrievedFromRingBufferInBytes == bytesToTake ? ESP_OK : ESP_ERR_INVALID_SIZE;
            if (err != ESP_OK) {
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                ESP_LOGW(BtI2sRingbufferTag, "take_from_ringbuffer_and_write_to_i2s() - Ringbuffer WRAP AROUND - Retrieved %u bytes out of %u bytes", sizeRetrievedFromRingBufferInBytes, bytesToTake);
#endif
                size_t bytesTakenFromRingBuffer = sizeRetrievedFromRingBufferInBytes;
                size_t bytesRemainingToTake = bytesToTake - bytesTakenFromRingBuffer;
                data = xRingbufferReceiveUpTo(s_i2s_ringbuffer, &sizeRetrievedFromRingBufferInBytes, ReadWaitTimeInTicks, bytesRemainingToTake);
                if (data != NULL) {
                    memcpy(s_i2s_audio_processing_buffer + bytesTakenFromRingBuffer, data, sizeRetrievedFromRingBufferInBytes);
                    vRingbufferReturnItem(s_i2s_ringbuffer, data);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
                    ESP_LOGI(BtI2sRingbufferTag, "take_from_ringbuffer_and_write_to_i2s() - Ringbuffer RE-READ - Retrieved %u bytes out of %u bytes", sizeRetrievedFromRingBufferInBytes, bytesRemainingToTake);
#endif
                    // Calculate the total number of bytes acquired after the second read
                    sizeRetrievedFromRingBufferInBytes += bytesTakenFromRingBuffer;
                    err = sizeRetrievedFromRingBufferInBytes == bytesToTake ? ESP_OK : ESP_ERR_INVALID_SIZE;
                    if (err != ESP_OK) {
                        ESP_LOGE(BtI2sRingbufferTag, "take_from_ringbuffer_and_write_to_i2s() - WRAP AROUND Compensation INCOMPLETE - TOTAL %u bytes of %u bytes", sizeRetrievedFromRingBufferInBytes, bytesToTake);
                    }
                } else {
                    ESP_LOGE(BtI2sRingbufferTag, "take_from_ringbuffer_and_write_to_i2s() - WRAP AROUND xRingbufferReceiveUpTo() failed");
                    err = ESP_ERR_TIMEOUT;
                }
            }

            // Data has been acquired and is a multiple of audio samples - Apply processing and write to I2S
            if (err == ESP_OK) {
                apply_volume(s_i2s_audio_processing_buffer, sizeRetrievedFromRingBufferInBytes, s_bytes_per_sample_per_channel);

                size_t bytesWritten = 0;
                err = i2s_channel_write(s_i2s_tx_channel, (void*) s_i2s_audio_processing_buffer, sizeRetrievedFromRingBufferInBytes, &bytesWritten, portMAX_DELAY);
                if (err != ESP_OK) {
                    ESP_LOGE(BtI2sOutputTag, "i2s_channel_write() failed with %d - Attempted to write %u bytes", err, sizeRetrievedFromRingBufferInBytes);
                }
            }
        } else {
            ESP_LOGW(BtI2sOutputTag, "xRingbufferReceiveUpTo() Ring buffer data read timeout - Attempted to read %u bytes", bytesToTake);
            err = ESP_ERR_TIMEOUT;
        }
    }

    return err;
}

static void apply_volume(void* data, size_t len, uint8_t bytePerSample) {
    const uint8_t volume_avrc = get_volume_avrc();
    const float volume_factor = get_volume_factor();

    // TODO: We currently only support I2S_DATA_BIT_WIDTH_16BIT or 2 bytes per channel
    uint16_t* incomingData = (uint16_t*) data;

    // Optimization: When volume is 0, we can just zero the buffer otherwise apply the desired factor to each sample
    if (volume_avrc == 0) {
        memset(data, 0, len);
    } else {
        for (size_t dataIndex = 0; dataIndex < len / bytePerSample; dataIndex++) {
            int32_t pcmDataWithVolume = (int32_t) lroundf((int16_t) incomingData[dataIndex] * volume_factor);
            
            // Clamp the result to [INT16_MIN, INT16_MAX] so we do not exceed the valid PCM range
            if (pcmDataWithVolume > INT16_MAX) {
                pcmDataWithVolume = INT16_MAX;
            } else if (pcmDataWithVolume < INT16_MIN) {
                pcmDataWithVolume = INT16_MIN;
            }

            incomingData[dataIndex] = (uint16_t) pcmDataWithVolume;
        }
    }
}

static void drain_ringbuffer() {
    bool doneDraining = false;
    do {
        // Retrieve the number of available bytes - We can retrieve all the buffer in one go since we are draining
        UBaseType_t bytesWaitingToBeRetrieved = 0;
        vRingbufferGetInfo(s_i2s_ringbuffer, NULL, NULL, NULL, NULL, &bytesWaitingToBeRetrieved);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        ESP_LOGI(BtI2sRingbufferTag, "drain_ringbuffer() - In buffer %u bytes", bytesWaitingToBeRetrieved);
#endif
        if (bytesWaitingToBeRetrieved > 0) {
            // ------------------------------------------------------------------------------------------------
            // xRingbufferReceiveUpTo() on ESP32 was measured as follows (for a buffer of 4096 bytes of data):
            //  - Average time:  3 us
            //  - Minimum time:  3 us
            //  - Maximum time: 18 us
            //
            // With the default FreeRTOS configuration (configTICK_RATE_HZ = 100Hz):
            //  - Anything below 1000 us is less than 1 FreeRTOS tick
            //
            // We choose `ReadWaitTimeInTicks` below as 10 times the maximum xRingbufferReceiveUpTo() time
            // ------------------------------------------------------------------------------------------------
            const TickType_t ReadWaitTimeInTicks = pdMS_TO_TICKS(10 * 1);
            size_t sizeRetrievedFromRingBufferInBytes = 0;
            void* data = xRingbufferReceiveUpTo(s_i2s_ringbuffer, &sizeRetrievedFromRingBufferInBytes, ReadWaitTimeInTicks, bytesWaitingToBeRetrieved);
            
#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
            ESP_LOGI(BtI2sRingbufferTag, "drain_ringbuffer() - xRingbufferReceiveUpTo() -> %s - Retrieved %u bytes out of %u bytes", data != NULL ? "SUCCESS" : "FAILED", sizeRetrievedFromRingBufferInBytes, bytesWaitingToBeRetrieved);
#endif

            if (data != NULL) {
                vRingbufferReturnItem(s_i2s_ringbuffer, data);
            }
        }

        doneDraining = bytesWaitingToBeRetrieved == 0;

    } while (!doneDraining);
}

static esp_err_t notify_a2dp_audio_active() {
    return notify_i2s_task(A2DPAudioStateActive);
}

static esp_err_t notify_a2dp_audio_paused() {
    return notify_i2s_task(A2DPAudioStatePaused);
}

static esp_err_t notify_i2s_task(a2dp_audio_state_t audioState) {
    // Update the current audio state
    atomic_exchange(&s_atomic_current_audio_state, audioState);

    esp_err_t err = ESP_OK;

    // If the audio is switching to "Active", wake up the I2S processing task
    if (audioState == A2DPAudioStateActive) {
#if CONFIG_HOLIDAYTREE_I2S_OUTPUT_LOG
            ESP_LOGI(BtI2sRingbufferTag, "Notifying I2S task -> Slot %d - Value 0x%"PRIu32, I2STaskNotificationIndex, I2STaskNotificationValue);
#endif
        const BaseType_t outcome = xTaskNotifyIndexed(s_i2s_task_handle, I2STaskNotificationIndex, I2STaskNotificationValue, eSetValueWithOverwrite);
        err = outcome == pdPASS ? ESP_OK : ESP_FAIL;
    }

    return err;
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


#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG

static const char* get_ringbuffer_mode_name(ringbuffer_mode_t ringbufferMode) {
    switch (ringbufferMode) {
        case RingbufferNone:
            return "RingbufferNone";
        case RingbufferPrefetching:
            return "RingbufferPrefetching";
        case RingbufferWriting:
            return "RingbufferWriting";
        default:
            return "N/A";
    }
}

#endif
