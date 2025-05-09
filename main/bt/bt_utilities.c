// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>


#include "bt/a2d_sbc_constants.h"
#include "bt/bt_utilities.h"


const char* get_gap_link_key_type_name(esp_bt_link_key_type_t linkKeyType) {
    switch (linkKeyType) {
        case ESP_BT_LINK_KEY_COMB:
            return "ESP_BT_LINK_KEY_COMB";
        case ESP_BT_LINK_KEY_DBG_COMB:
            return "ESP_BT_LINK_KEY_DBG_COMB";
        case ESP_BT_LINK_KEY_UNAUTHED_COMB_P192:
            return "ESP_BT_LINK_KEY_UNAUTHED_COMB_P192";
        case ESP_BT_LINK_KEY_AUTHED_COMB_P192:
            return "ESP_BT_LINK_KEY_AUTHED_COMB_P192";
        case ESP_BT_LINK_KEY_CHG_COMB:
            return "ESP_BT_LINK_KEY_CHG_COMB";
        case ESP_BT_LINK_KEY_UNAUTHED_COMB_P256:
            return "ESP_BT_LINK_KEY_UNAUTHED_COMB_P256";
        case ESP_BT_LINK_KEY_AUTHED_COMB_P256:
            return "ESP_BT_LINK_KEY_AUTHED_COMB_P256";
        default:
            return "N/A";
    }
}

const char* get_gap_encryption_mode(esp_bt_enc_mode_t encryptionMode) {
    switch (encryptionMode) {
        case ESP_BT_ENC_MODE_OFF:
            return "ESP_BT_ENC_MODE_OFF";
        case ESP_BT_ENC_MODE_E0:
            return "ESP_BT_ENC_MODE_E0";
        case ESP_BT_ENC_MODE_AES:
            return "ESP_BT_ENC_MODE_AES";
        default:
            return "N/A";
    }
}

const char* get_gap_power_mode_name(esp_bt_pm_mode_t powerMode) {
    switch (powerMode) {
        case ESP_BT_PM_MD_ACTIVE:
            return "ESP_BT_PM_MD_ACTIVE";
        case ESP_BT_PM_MD_HOLD:
            return "ESP_BT_PM_MD_HOLD";
        case ESP_BT_PM_MD_SNIFF:
            return "ESP_BT_PM_MD_SNIFF";
        case ESP_BT_PM_MD_PARK:
            return "ESP_BT_PM_MD_PARK";
        default:
            return "N/A";
    }
}

const char* get_eir_name(esp_bt_eir_type_t eir) {
    switch (eir) {
        case ESP_BT_EIR_TYPE_FLAGS:
            return "ESP_BT_EIR_TYPE_FLAGS";
        case ESP_BT_EIR_TYPE_INCMPL_16BITS_UUID:
            return "ESP_BT_EIR_TYPE_INCMPL_16BITS_UUID";
        case ESP_BT_EIR_TYPE_CMPL_16BITS_UUID:
            return "ESP_BT_EIR_TYPE_CMPL_16BITS_UUID";
        case ESP_BT_EIR_TYPE_INCMPL_32BITS_UUID:
            return "ESP_BT_EIR_TYPE_INCMPL_32BITS_UUID";
        case ESP_BT_EIR_TYPE_CMPL_32BITS_UUID:
            return "ESP_BT_EIR_TYPE_CMPL_32BITS_UUID";
        case ESP_BT_EIR_TYPE_INCMPL_128BITS_UUID:
            return "ESP_BT_EIR_TYPE_INCMPL_128BITS_UUID";
        case ESP_BT_EIR_TYPE_CMPL_128BITS_UUID:
            return "ESP_BT_EIR_TYPE_CMPL_128BITS_UUID";
        case ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME:
            return "ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME";
        case ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME:
            return "ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME";
        case ESP_BT_EIR_TYPE_TX_POWER_LEVEL:
            return "ESP_BT_EIR_TYPE_TX_POWER_LEVEL";
        case ESP_BT_EIR_TYPE_URL:
            return "ESP_BT_EIR_TYPE_URL";
        case ESP_BT_EIR_TYPE_MANU_SPECIFIC:
            return "ESP_BT_EIR_TYPE_MANU_SPECIFIC";
        default:
            return "N/A";
    }
}

const char* get_avrc_metdata_attribute_name(uint8_t attributeId) {
    switch (attributeId) {
        case ESP_AVRC_MD_ATTR_TITLE:
            return "ESP_AVRC_MD_ATTR_TITLE";
        case ESP_AVRC_MD_ATTR_ARTIST:
            return "ESP_AVRC_MD_ATTR_ARTIST";
        case ESP_AVRC_MD_ATTR_ALBUM:
            return "ESP_AVRC_MD_ATTR_ALBUM";
        case ESP_AVRC_MD_ATTR_TRACK_NUM:
            return "ESP_AVRC_MD_ATTR_TRACK_NUM";
        case ESP_AVRC_MD_ATTR_NUM_TRACKS:
            return "ESP_AVRC_MD_ATTR_NUM_TRACKS";
        case ESP_AVRC_MD_ATTR_GENRE:
            return "ESP_AVRC_MD_ATTR_GENRE";
        case ESP_AVRC_MD_ATTR_PLAYING_TIME:
            return "ESP_AVRC_MD_ATTR_PLAYING_TIME";
        default:
            return "N/A";
    }
}

const char* get_avrc_notification_name(uint8_t eventId) {
    switch (eventId) {
        case ESP_AVRC_RN_PLAY_STATUS_CHANGE:
            return "ESP_AVRC_RN_PLAY_STATUS_CHANGE";
        case ESP_AVRC_RN_TRACK_CHANGE:
            return "ESP_AVRC_RN_TRACK_CHANGE";
        case ESP_AVRC_RN_TRACK_REACHED_END:
            return "ESP_AVRC_RN_TRACK_REACHED_END";
        case ESP_AVRC_RN_TRACK_REACHED_START:
            return "ESP_AVRC_RN_TRACK_REACHED_START";
        case ESP_AVRC_RN_PLAY_POS_CHANGED:
            return "ESP_AVRC_RN_PLAY_POS_CHANGED";
        case ESP_AVRC_RN_BATTERY_STATUS_CHANGE:
            return "ESP_AVRC_RN_BATTERY_STATUS_CHANGE";
        case ESP_AVRC_RN_SYSTEM_STATUS_CHANGE:
            return "ESP_AVRC_RN_SYSTEM_STATUS_CHANGE";
        case ESP_AVRC_RN_APP_SETTING_CHANGE:
            return "ESP_AVRC_RN_APP_SETTING_CHANGE";
        case ESP_AVRC_RN_NOW_PLAYING_CHANGE:
            return "ESP_AVRC_RN_NOW_PLAYING_CHANGE";
        case ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE:
            return "ESP_AVRC_RN_AVAILABLE_PLAYERS_CHANGE";
        case ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE:
            return "ESP_AVRC_RN_ADDRESSED_PLAYER_CHANGE";
        case ESP_AVRC_RN_UIDS_CHANGE:
            return "ESP_AVRC_RN_UIDS_CHANGE";
        case ESP_AVRC_RN_VOLUME_CHANGE:
            return "ESP_AVRC_RN_VOLUME_CHANGE";
        default:
            return "N/A";
    }
}

const char* get_avrc_playback_stat_name(esp_avrc_playback_stat_t playbackStat) {
    switch (playbackStat) {
        case ESP_AVRC_PLAYBACK_STOPPED:
            return "ESP_AVRC_PLAYBACK_STOPPED";
        case ESP_AVRC_PLAYBACK_PLAYING:
            return "ESP_AVRC_PLAYBACK_PLAYING";
        case ESP_AVRC_PLAYBACK_PAUSED:
            return "ESP_AVRC_PLAYBACK_PAUSED";
        case ESP_AVRC_PLAYBACK_FWD_SEEK:
            return "ESP_AVRC_PLAYBACK_FWD_SEEK";
        case ESP_AVRC_PLAYBACK_REV_SEEK:
            return "ESP_AVRC_PLAYBACK_REV_SEEK";
        case ESP_AVRC_PLAYBACK_ERROR:
            return "ESP_AVRC_PLAYBACK_ERROR";
        default:
            return "N/A";
    }
}

const char* get_avrc_battery_stat_name(esp_avrc_batt_stat_t batteryStat) {
    switch (batteryStat) {
        case ESP_AVRC_BATT_NORMAL:
            return "ESP_AVRC_BATT_NORMAL";
        case ESP_AVRC_BATT_WARNING:
            return "ESP_AVRC_BATT_WARNING";
        case ESP_AVRC_BATT_CRITICAL:
            return "ESP_AVRC_BATT_CRITICAL";
        case ESP_AVRC_BATT_EXTERNAL:
            return "ESP_AVRC_BATT_EXTERNAL";
        default:
            return "N/A";
    }
}

char** get_avrc_feature_names(uint32_t featureMask, char* featuresStr[6]) {
    if (featuresStr == NULL) {
        return NULL;
    }

    memset(featuresStr, 0,6 * sizeof(featuresStr[0]));
    uint8_t index = 0;

    if ((featureMask & ESP_AVRC_FEAT_ADV_CTRL) == ESP_AVRC_FEAT_ADV_CTRL) {
        featuresStr[index++] = "ESP_AVRC_FEAT_ADV_CTRL (0x0200)";
    }
    
    if ((featureMask & ESP_AVRC_FEAT_META_DATA) == ESP_AVRC_FEAT_META_DATA) {
        featuresStr[index++] = "ESP_AVRC_FEAT_META_DATA (0x0040)";
    }

    if ((featureMask & ESP_AVRC_FEAT_BROWSE) == ESP_AVRC_FEAT_BROWSE) {
        featuresStr[index++] = "ESP_AVRC_FEAT_BROWSE (0x0010)";
    }

    if ((featureMask & ESP_AVRC_FEAT_VENDOR) == ESP_AVRC_FEAT_VENDOR) {
        featuresStr[index++] = "ESP_AVRC_FEAT_VENDOR (0x0008)";
    }

    if ((featureMask & ESP_AVRC_FEAT_RCCT) == ESP_AVRC_FEAT_RCCT) {
        featuresStr[index++] = "ESP_AVRC_FEAT_RCCT (0x0002)";
    }

    if ((featureMask & ESP_AVRC_FEAT_RCTG) == ESP_AVRC_FEAT_RCTG) {
        featuresStr[index++] = "ESP_AVRC_FEAT_RCTG (0x0001)";
    }

    return featuresStr;
}

char** get_avrc_controller_feature_flags(uint16_t featureFlags, char* featuresStr[8]) {
    if (featuresStr == NULL) {
        return NULL;
    }

    memset(featuresStr, 0, 8 * sizeof(featuresStr[0]));
    uint8_t index = 0;

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_COVER_ART_GET_LINKED_THUMBNAIL) == ESP_AVRC_FEAT_FLAG_COVER_ART_GET_LINKED_THUMBNAIL) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_COVER_ART_GET_LINKED_THUMBNAIL (0x0200)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_COVER_ART_GET_IMAGE) == ESP_AVRC_FEAT_FLAG_COVER_ART_GET_IMAGE) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_COVER_ART_GET_IMAGE (0x0100)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_COVER_ART_GET_IMAGE_PROP) == ESP_AVRC_FEAT_FLAG_COVER_ART_GET_IMAGE_PROP) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_COVER_ART_GET_IMAGE_PROP (0x0080)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_BROWSING) == ESP_AVRC_FEAT_FLAG_BROWSING) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_BROWSING (0x0040)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT4) == ESP_AVRC_FEAT_FLAG_CAT4) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT4 (0x0008)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT3) == ESP_AVRC_FEAT_FLAG_CAT3) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT3 (0x0004)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT2) == ESP_AVRC_FEAT_FLAG_CAT2) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT2 (0x0002)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT1) == ESP_AVRC_FEAT_FLAG_CAT1) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT1 (0x0001)";
    }

    return featuresStr;
}

char** get_avrc_target_feature_flags(uint16_t featureFlags, char* featuresStr[6]) {
    if (featuresStr == NULL) {
        return NULL;
    }

    memset(featuresStr, 0, 6 * sizeof(featuresStr[0]));
    uint8_t index = 0;

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_TG_COVER_ART) == ESP_AVRC_FEAT_FLAG_TG_COVER_ART) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_TG_COVER_ART (0x0100)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_BROWSING) == ESP_AVRC_FEAT_FLAG_BROWSING) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_BROWSING (0x0040)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT4) == ESP_AVRC_FEAT_FLAG_CAT4) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT4 (0x0008)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT3) == ESP_AVRC_FEAT_FLAG_CAT3) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT3 (0x0004)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT2) == ESP_AVRC_FEAT_FLAG_CAT2) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT2 (0x0002)";
    }

    if ((featureFlags & ESP_AVRC_FEAT_FLAG_CAT1) == ESP_AVRC_FEAT_FLAG_CAT1) {
        featuresStr[index++] = "ESP_AVRC_FEAT_FLAG_CAT1 (0x0001)";
    }

    return featuresStr;
}

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
        case A2D_SBC_IE_SAMP_FREQ_16:
            return "A2D_SBC_IE_SAMP_FREQ_16";
        case A2D_SBC_IE_SAMP_FREQ_32:
            return "A2D_SBC_IE_SAMP_FREQ_32";
        case A2D_SBC_IE_SAMP_FREQ_44:
            return "A2D_SBC_IE_SAMP_FREQ_44";
        case A2D_SBC_IE_SAMP_FREQ_48:
            return "A2D_SBC_IE_SAMP_FREQ_48";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_channel_mode_name(uint8_t channelMode) {
    switch (channelMode){
        case A2D_SBC_IE_CH_MD_MONO:
            return "A2D_SBC_IE_CH_MD_MONO";
        case A2D_SBC_IE_CH_MD_DUAL:
            return "A2D_SBC_IE_CH_MD_DUAL";
        case A2D_SBC_IE_CH_MD_STEREO:
            return "A2D_SBC_IE_CH_MD_STEREO";
        case A2D_SBC_IE_CH_MD_JOINT:
            return "A2D_SBC_IE_CH_MD_JOINT";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_block_count_name(uint8_t blockCount) {
    switch (blockCount) {
        case A2D_SBC_IE_BLOCKS_4:
            return "A2D_SBC_IE_BLOCKS_4";
        case A2D_SBC_IE_BLOCKS_8:
            return "A2D_SBC_IE_BLOCKS_8";
        case A2D_SBC_IE_BLOCKS_12:
            return "A2D_SBC_IE_BLOCKS_12";
        case A2D_SBC_IE_BLOCKS_16:
            return "A2D_SBC_IE_BLOCKS_16";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_subbands_name(uint8_t subBands) {
    switch (subBands) {
        case A2D_SBC_IE_SUBBAND_4:
            return "A2D_SBC_IE_SUBBAND_4";
        case A2D_SBC_IE_SUBBAND_8:
            return "A2D_SBC_IE_SUBBAND_8";
        default:
            return "N/A";
    }
}

const char* get_a2d_sbc_allocation_mode(uint8_t allocationMode) {
    switch (allocationMode) {
        case A2D_SBC_IE_ALLOC_MD_S:
            return "A2D_SBC_IE_ALLOC_MD_S (SNR)";
        case A2D_SBC_IE_ALLOC_MD_L:
            return "A2D_SBC_IE_ALLOC_MD_L (loundess)";
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

const char* get_a2d_protocol_service_capabilities_name(esp_a2d_psc_t capabilitiesMask) {
    if ((capabilitiesMask & ESP_A2D_PSC_DELAY_RPT) == ESP_A2D_PSC_DELAY_RPT) {
        return "ESP_A2D_PSC_DELAY_RPT";
    }

    return "";
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
        case ESP_A2D_MEDIA_CTRL_STOP:
            return "ESP_A2D_MEDIA_CTRL_STOP";
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