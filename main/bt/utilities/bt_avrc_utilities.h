// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_avrc_api.h>


const char* get_avrc_metdata_attribute_name(uint8_t attributeId);
const char* get_avrc_notification_name(uint8_t eventId);
const char* get_avrc_playback_stat_name(esp_avrc_playback_stat_t playbackStat);
const char* get_avrc_battery_stat_name(esp_avrc_batt_stat_t batteryStat);
char** get_avrc_feature_names(uint32_t featureMask, char* featuresStr[6]);
char** get_avrc_controller_feature_flags(uint16_t featureFlags, char* featuresStr[8]);
char** get_avrc_target_feature_flags(uint16_t featureFlags, char* featuresStr[6]);