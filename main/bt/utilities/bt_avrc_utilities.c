// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>


#include "bt/utilities/bt_avrc_utilities.h"

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