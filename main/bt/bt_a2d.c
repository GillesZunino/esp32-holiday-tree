// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <esp_check.h>
#include <esp_log.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>


#include "bt/a2d_sbc_constants.h"
#include "bt/bt_utilities.h"
#include "bt/bt_work_dispatcher.h"
#include "bt/i2s_output.h"
#include "bt/bt_a2d.h"



// Bluetooth A2D log tag
static const char* BtA2dTag = "bt_a2d";


// Bluetooth A2D application layer delay in 1/10 ms units
static const uint16_t ApplicationDelayInOneOverTenMs = 5 * 10; // 50 * 1/10 ms = 5 ms


static void a2d_event_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* params);
static void a2d_data_sink_callback(const uint8_t* data, uint32_t len);

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
static void log_icoming_a2d_data_stats(uint32_t len);
static void reset_incoming_a2d_data_stats(const esp_a2d_cb_param_t* const params);
#endif

static void a2d_event_handler(uint16_t event, void *param);

static uint32_t get_sample_frequency(uint8_t sampleFrequency);
static uint8_t get_channel_count(uint8_t channelModeBits);


esp_err_t setup_a2d_profile() {
    // Initialize Advanced Audio
    ESP_RETURN_ON_ERROR(esp_a2d_sink_init(), BtA2dTag, "esp_a2d_sink_init() failed");

    // Register A2D command and status callback
    ESP_RETURN_ON_ERROR(esp_a2d_register_callback(&a2d_event_callback), BtA2dTag, "esp_a2d_register_callback() failed");

    // Register A2D data callback
    ESP_RETURN_ON_ERROR(esp_a2d_sink_register_data_callback(a2d_data_sink_callback), BtA2dTag, "esp_a2d_sink_register_data_callback() failed");

    // Get the default delay - The response comes through the callback
    ESP_RETURN_ON_ERROR(esp_a2d_sink_get_delay_value(), BtA2dTag, "esp_a2d_sink_get_delay_value()");
    return ESP_OK;
}

static void a2d_event_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* params) {
    bool eventEnqueued = queue_bluetooth_workitem(a2d_event_handler, event, params, sizeof(esp_a2d_cb_param_t));
    if (!eventEnqueued) {
        ESP_LOGE(BtA2dTag, "%s() could not queue event %d to Bluetooth dispatcher", __func__, event);
    }
}

static void a2d_data_sink_callback(const uint8_t* data, uint32_t len) {
    uint32_t byteWritten = write_to_i2s_output(data, len);
    if (byteWritten != len) {
        ESP_LOGW(BtA2dTag, "a2d_data_sink_callback() failed to write to I2S ring buffer. Expected size: 0x%"PRIu32", Written size: 0x%"PRIu32, len, byteWritten);
    }

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
    log_icoming_a2d_data_stats(len);
#endif
}

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG

// Keep track of the number of audio packets received - Only in development
static uint32_t s_audio_packets_count = 0;

// Keep track of the average number of bytes per sample to write - Only in development
static uint32_t s_audio_average_packet_size = 0;
static uint64_t s_audio_total_bytes_received = 0;


static void log_icoming_a2d_data_stats(uint32_t len) {
    s_audio_packets_count++;
    
    s_audio_total_bytes_received += len;
    s_audio_average_packet_size = s_audio_total_bytes_received /  s_audio_packets_count;

    if (s_audio_packets_count % 100 == 0) {
        ESP_LOGI(BtA2dTag, "Audio packet count %"PRIu32" - Average buffer size %"PRIu32, s_audio_packets_count, s_audio_average_packet_size);
    }
}

static void reset_incoming_a2d_data_stats(const esp_a2d_cb_param_t* const params) {
    if (params->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
        s_audio_packets_count = 0;
        s_audio_average_packet_size = 0;
        s_audio_total_bytes_received = 0;
    }
}

#endif

static void a2d_event_handler(uint16_t event, void* rawParams) {
    const esp_a2d_cb_param_t* const params = (const esp_a2d_cb_param_t * const) rawParams;
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            char bdaStr[18];
            ESP_LOGI(BtA2dTag, "ESP_A2D_CONNECTION_STATE_EVT %s remote [%s]", get_a2d_connection_state_name(params->conn_stat.state), get_bda_string(params->conn_stat.remote_bda, bdaStr));
#endif
            switch (params->conn_stat.state) {
                case ESP_A2D_CONNECTION_STATE_CONNECTING: {
                    esp_err_t err = create_i2s_output();
                    if (err != ESP_OK) {
                        char errMsg[64];
                        ESP_LOGE(BtA2dTag, "create_i2s_output() failed %s", esp_err_to_name_r(err, errMsg, sizeof(errMsg)));
                    }
                }
                break;

                case ESP_A2D_CONNECTION_STATE_CONNECTED: {
                    esp_err_t err = esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
                    if (err == ESP_OK) {
                        err = start_i2s_output();
                        if (err != ESP_OK) {
                            char errMsg[64];
                            ESP_LOGE(BtA2dTag, "start_i2s_output() failed %s", esp_err_to_name_r(err, errMsg, sizeof(errMsg)));
                        }
                    } else {
                        char errMsg[64];
                        ESP_LOGE(BtA2dTag, "esp_bt_gap_set_scan_mode() failed %s", esp_err_to_name_r(err, errMsg, sizeof(errMsg)));
                    }
                }
                break;

                case ESP_A2D_CONNECTION_STATE_DISCONNECTED: {
                    // Shutdown I2S output
                    esp_err_t err = delete_i2s_output();
                    if (err != ESP_OK) {
                        char errMsg[64];
                        ESP_LOGE(BtA2dTag, "delete_i2s_output() failed %s", esp_err_to_name_r(err, errMsg, sizeof(errMsg)));
                    }

                    // Make device discoverable again so a new connection can be established
                    err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
                    if (err != ESP_OK) {
                        char errMsg[64];
                        ESP_LOGE(BtA2dTag, "esp_bt_gap_set_scan_mode() failed %s", esp_err_to_name_r(err, errMsg, sizeof(errMsg)));
                    }
                }
                break;

                default:
                    break;
            }
        }
        break;

        case ESP_A2D_AUDIO_STATE_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            ESP_LOGI(BtA2dTag, "ESP_A2D_AUDIO_STATE_EVT %s", get_a2d_audio_state_name(params->audio_stat.state));
#endif
            esp_err_t err = set_i2s_output_audio_state(params->audio_stat.state);
            if (err != ESP_OK) {
                ESP_LOGE(BtA2dTag, "ESP_A2D_AUDIO_STATE_EVT %s - Failed to set_i2s_output_audio_state() with %d", get_a2d_audio_state_name(params->audio_stat.state), err);
            }

#if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
            reset_incoming_a2d_data_stats(params);
#endif
        }
        break;

        case ESP_A2D_AUDIO_CFG_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            ESP_LOGI(BtA2dTag, "ESP_A2D_AUDIO_CFG_EVT codec types (0x%x)", params->audio_cfg.mcc.type);
            char* codecStr[5];
            get_a2d_media_codec_names(params->audio_cfg.mcc.type, codecStr);
            for (uint8_t index = 0; (codecStr[index] != NULL) && (index < 5); index++) {
                ESP_LOGI(BtA2dTag, "\t%s", codecStr[index]);
            }
#endif

            // We currently only support the SBC codec
            switch (params->audio_cfg.mcc.type) {
                case ESP_A2D_MCT_SBC: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
                    ESP_LOGI(BtA2dTag, "ESP_A2D_AUDIO_CFG_EVT - SBC codec configuration 0x%x-0x%x-0x%x-0x%x",
                        params->audio_cfg.mcc.cie.sbc[0],
                        params->audio_cfg.mcc.cie.sbc[1],
                        params->audio_cfg.mcc.cie.sbc[2],
                        params->audio_cfg.mcc.cie.sbc[3]);
#endif

                    // Sample frequency
                    uint8_t samplingFrequencyBits = params->audio_cfg.mcc.cie.sbc[0] & A2D_SBC_IE_SAMP_FREQ_MSK;
                    uint32_t sampleFrequency = get_sample_frequency(samplingFrequencyBits);

#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
                    ESP_LOGI(BtA2dTag, "\tSample frequency %s (%lu)", get_a2d_sbc_sample_frequency_name(samplingFrequencyBits), sampleFrequency);
#endif

                    // Channel mode
                    uint8_t channelModeBits = params->audio_cfg.mcc.cie.sbc[0] & A2D_SBC_IE_CH_MD_MSK;
                    uint8_t channelCount = get_channel_count(channelModeBits);

#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
                    ESP_LOGI(BtA2dTag, "\tChannel mode %s (0x%x) - Channel count %d", get_a2d_sbc_channel_mode_name(channelModeBits), channelModeBits, channelCount);

                    // Blocks and sub bands
                    uint8_t blocksCountBits = params->audio_cfg.mcc.cie.sbc[1] & A2D_SBC_IE_BLOCKS_MSK;
                    uint8_t subbandsBits = params->audio_cfg.mcc.cie.sbc[1] & A2D_SBC_IE_SUBBAND_MSK;
                    ESP_LOGI(BtA2dTag, "\tBlocks %s (0x%x) - Sub bands %s (0x%x)", get_a2d_sbc_block_count_name(blocksCountBits), blocksCountBits, get_a2d_sbc_subbands_name(subbandsBits), subbandsBits);

                    // Allocation mode
                    uint8_t allocationModeBits = params->audio_cfg.mcc.cie.sbc[1] & A2D_SBC_IE_ALLOC_MD_MSK;
                    ESP_LOGI(BtA2dTag, "\tAllocation mode %s (0x%x)", get_a2d_sbc_allocation_mode(allocationModeBits), allocationModeBits);

                    // Min and max bit pool
                    ESP_LOGI(BtA2dTag, "\tESP_A2D_AUDIO_CFG_EVT - SBC codec min bit pool %d | max bit pool %d", params->audio_cfg.mcc.cie.sbc[2], params->audio_cfg.mcc.cie.sbc[3]);
#endif

                    // Configure I2S output with the paramters extracted from the codec configuration - SBC is always 16 bits data
                    esp_err_t err = configure_i2s_output(sampleFrequency, I2S_DATA_BIT_WIDTH_16BIT, channelCount);
                    if (err != ESP_OK) {
                        char err_msg[64];
                        ESP_LOGE(BtA2dTag, "configure_i2s_output() failed. No audio will play - %s", esp_err_to_name_r(err, err_msg, sizeof(err_msg)));
                    }
                }
                break;

                default:
                    ESP_LOGW(BtA2dTag, "ESP_A2D_AUDIO_CFG_EVT unsupported codec (0x%x)", params->audio_cfg.mcc.type);
                break;
            }
        }
        break;

        case ESP_A2D_MEDIA_CTRL_ACK_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            ESP_LOGI(BtA2dTag, "ESP_A2D_MEDIA_CTRL_ACK_EVT %s - %s", get_a2d_media_command_name(params->media_ctrl_stat.cmd), get_a2d_media_command_ack_name(params->media_ctrl_stat.status));
#endif   
        }
        break;

        case ESP_A2D_PROF_STATE_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            ESP_LOGI(BtA2dTag, "ESP_A2D_PROF_STATE_EVT %s (0x%x)", get_a2d_init_state_name(params->a2d_prof_stat.init_state), params->a2d_prof_stat.init_state);
#endif
        }
        break;

        case ESP_A2D_SNK_PSC_CFG_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            ESP_LOGI(BtA2dTag, "ESP_A2D_SNK_PSC_CFG_EVT %s (0x%02x)", get_a2d_protocol_service_capabilities_name(params->a2d_psc_cfg_stat.psc_mask), params->a2d_psc_cfg_stat.psc_mask);
            bool delayReportingSupported = (params->a2d_psc_cfg_stat.psc_mask & ESP_A2D_PSC_DELAY_RPT) == ESP_A2D_PSC_DELAY_RPT;
            ESP_LOGI(BtA2dTag, "\tDelay reporting %s", delayReportingSupported ? "supported" : "UNsupported");
#endif
        }
        break;

        case ESP_A2D_SNK_SET_DELAY_VALUE_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            if (params->a2d_set_delay_value_stat.set_state == ESP_A2D_SET_SUCCESS) {
                ESP_LOGI(BtA2dTag, "ESP_A2D_SNK_SET_DELAY_VALUE_EVT delay value %u (in 1/10 ms), %u", params->a2d_set_delay_value_stat.delay_value, params->a2d_set_delay_value_stat.delay_value / 10);
            } else {
                ESP_LOGI(BtA2dTag, "ESP_A2D_SNK_SET_DELAY_VALUE_EVT failed");
            }
#endif
        }
        break;

        case ESP_A2D_SNK_GET_DELAY_VALUE_EVT: {
#if CONFIG_HOLIDAYTREE_BT_A2DP_LOG
            ESP_LOGI(BtA2dTag, "ESP_A2D_SNK_GET_DELAY_VALUE_EVT delay value %u (in 1/10 ms), %u", params->a2d_get_delay_value_stat.delay_value, params->a2d_get_delay_value_stat.delay_value / 10);
#endif
            esp_err_t err = esp_a2d_sink_set_delay_value(params->a2d_get_delay_value_stat.delay_value + ApplicationDelayInOneOverTenMs);
            if (err != ESP_OK) {
                char err_msg[64];
                ESP_LOGE(BtA2dTag, "a2d_event_handler() failed - Unable to esp_a2d_sink_set_delay_value() %s", esp_err_to_name_r(err, err_msg, sizeof(err_msg)));
            }
        }
        break;

        default:
            ESP_LOGI(BtA2dTag, "%s() received unknown event '%d'", __func__, event);
            break;
    }
}

static uint32_t get_sample_frequency(uint8_t sampleFrequency) {
    switch (sampleFrequency) {
        case A2D_SBC_IE_SAMP_FREQ_16:
            return 16000;
        case A2D_SBC_IE_SAMP_FREQ_32:
            return 32000;
        case A2D_SBC_IE_SAMP_FREQ_44:
            return 44100;
        case A2D_SBC_IE_SAMP_FREQ_48:
            return 48000;
        default:
            return 0;
    }
}

static uint8_t get_channel_count(uint8_t channelModeBits) {
    switch (channelModeBits) {
        case A2D_SBC_IE_CH_MD_MONO:
            return 1;
        case A2D_SBC_IE_CH_MD_DUAL:
        case A2D_SBC_IE_CH_MD_STEREO:
        case A2D_SBC_IE_CH_MD_JOINT:
            return 2;
        default:
            return 0;
    }
}