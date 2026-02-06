// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include "bt/utilities/bt_a2d_utilities.h"


const char* get_a2d_connection_state_name(esp_a2d_connection_state_t connectionState) {
    switch (connectionState) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            return "ESP_A2D_CONNECTION_STATE_DISCONNECTED";
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            return "ESP_A2D_CONNECTION_STATE_CONNECTING";
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            return "ESP_A2D_CONNECTION_STATE_CONNECTED";
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            return "ESP_A2D_CONNECTION_STATE_DISCONNECTING";
        default:
            return "N/A";
    }
}

const char* get_a2d_protocol_service_capabilities_name(esp_a2d_psc_t capabilitiesMask) {
    if ((capabilitiesMask & ESP_A2D_PSC_DELAY_RPT) == ESP_A2D_PSC_DELAY_RPT) {
        return "ESP_A2D_PSC_DELAY_RPT";
    }

    return "";
}

const char* get_a2d_audio_state_name(esp_a2d_audio_state_t audioState) {
    switch (audioState) {
        case ESP_A2D_AUDIO_STATE_SUSPEND:
            return "ESP_A2D_AUDIO_STATE_SUSPEND";
        case ESP_A2D_AUDIO_STATE_STARTED:
            return "ESP_A2D_AUDIO_STATE_STARTED";
        default:
            return "N/A";
    }
}

char** get_a2d_media_codec_names(esp_a2d_mct_t codecType, char* codecNames[5]) {
    if (codecNames == NULL) {
        return NULL;
    }

    memset(codecNames, 0, 5 * sizeof(codecNames[0]));
    uint8_t index = 0;

    if ((codecType & ESP_A2D_MCT_SBC) == ESP_A2D_MCT_SBC) {
        codecNames[index++] = "ESP_A2D_MCT_SBC (0x00)";
    }

    if ((codecType & ESP_A2D_MCT_M12) == ESP_A2D_MCT_M12) {
        codecNames[index++] = "ESP_A2D_MCT_M12 (0x01)";
    }

    if ((codecType & ESP_A2D_MCT_M24) == ESP_A2D_MCT_M24) {
        codecNames[index++] = "ESP_A2D_MCT_M24 (0x02)";
    }

    if ((codecType & ESP_A2D_MCT_ATRAC) == ESP_A2D_MCT_ATRAC) {
        codecNames[index++] = "ESP_A2D_MCT_ATRAC (0x04)";
    }

    if ((codecType & ESP_A2D_MCT_NON_A2DP) == ESP_A2D_MCT_NON_A2DP) {
        codecNames[index++] = "ESP_A2D_MCT_NON_A2DP (0xFF)";
    }

    return codecNames;
}

const char* get_a2d_sbc_sample_frequency_name(uint8_t sampleFrequency) {
    switch (sampleFrequency) {
        case ESP_A2D_SBC_CIE_SF_16K:
            return "ESP_A2D_SBC_CIE_SF_16K";
        case ESP_A2D_SBC_CIE_SF_32K:
            return "ESP_A2D_SBC_CIE_SF_32K";
        case ESP_A2D_SBC_CIE_SF_44K:
            return "ESP_A2D_SBC_CIE_SF_44K";
        case ESP_A2D_SBC_CIE_SF_48K:
            return "ESP_A2D_SBC_CIE_SF_48K";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_channel_mode_name(uint8_t channelMode) {
    switch (channelMode){
        case ESP_A2D_SBC_CIE_CH_MODE_MONO:
            return "ESP_A2D_SBC_CIE_CH_MODE_MONO";
        case ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL:
            return "ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL";
        case ESP_A2D_SBC_CIE_CH_MODE_STEREO:
            return "ESP_A2D_SBC_CIE_CH_MODE_STEREO";
        case ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO:
            return "ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_block_count_name(uint8_t blockCount) {
    switch (blockCount) {
        case ESP_A2D_SBC_CIE_BLOCK_LEN_4:
            return "ESP_A2D_SBC_CIE_BLOCK_LEN_4";
        case ESP_A2D_SBC_CIE_BLOCK_LEN_8:
            return "ESP_A2D_SBC_CIE_BLOCK_LEN_8";
        case ESP_A2D_SBC_CIE_BLOCK_LEN_12:
            return "ESP_A2D_SBC_CIE_BLOCK_LEN_12";
        case ESP_A2D_SBC_CIE_BLOCK_LEN_16:
            return "ESP_A2D_SBC_CIE_BLOCK_LEN_16";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_subbands_name(uint8_t subBands) {
    switch (subBands) {
        case ESP_A2D_SBC_CIE_NUM_SUBBANDS_4:
            return "ESP_A2D_SBC_CIE_NUM_SUBBANDS_4";
        case ESP_A2D_SBC_CIE_NUM_SUBBANDS_8:
            return "ESP_A2D_SBC_CIE_NUM_SUBBANDS_8";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_allocation_mode(uint8_t allocationMode) {
    switch (allocationMode) {
        case ESP_A2D_SBC_CIE_ALLOC_MTHD_SRN:
            return "ESP_A2D_SBC_CIE_ALLOC_MTHD_SRN (SRN)";
        case ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS:
            return "ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS (Loudness)";
        default:
            return "N/A";
    }
}

const char* get_a2d_init_state_name(esp_a2d_init_state_t initState) {
    switch (initState) {
        case ESP_A2D_DEINIT_SUCCESS:
            return "ESP_A2D_DEINIT_SUCCESS";
        case ESP_A2D_INIT_SUCCESS:
            return "ESP_A2D_INIT_SUCCESS";
        default:
            return "N/A";
    }
}

const char* get_a2d_media_command_name(esp_a2d_media_ctrl_t cmd) {
    switch (cmd) {
        case ESP_A2D_MEDIA_CTRL_NONE:
            return "ESP_A2D_MEDIA_CTRL_NONE";
        case ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY:
            return "ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY";
        case ESP_A2D_MEDIA_CTRL_START:
            return "ESP_A2D_MEDIA_CTRL_START";
        case ESP_A2D_MEDIA_CTRL_SUSPEND:
            return "ESP_A2D_MEDIA_CTRL_SUSPEND";
        default:
            return "N/A";
    }
}

const char* get_a2d_media_command_ack_name(esp_a2d_media_ctrl_ack_t ackStatus) {
    switch (ackStatus) {
        case ESP_A2D_MEDIA_CTRL_ACK_SUCCESS:
            return "ESP_A2D_MEDIA_CTRL_ACK_SUCCESS";
        case ESP_A2D_MEDIA_CTRL_ACK_FAILURE:
            return "ESP_A2D_MEDIA_CTRL_ACK_FAILURE";
        case ESP_A2D_MEDIA_CTRL_ACK_BUSY:
            return "ESP_A2D_MEDIA_CTRL_ACK_BUSY";
        default:
            return "N/A";
    }
}