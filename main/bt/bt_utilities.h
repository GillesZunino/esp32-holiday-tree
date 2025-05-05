// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_bt_defs.h>
#include <esp_gap_bt_api.h>
#include <esp_avrc_api.h>
#include <esp_a2dp_api.h>


const char* get_gap_link_key_type_name(esp_bt_link_key_type_t linkKeyType);
const char* get_gap_encryption_mode(esp_bt_enc_mode_t encryptionMode);
const char* get_gap_power_mode_name(esp_bt_pm_mode_t powerMode);

const char* get_eir_name(esp_bt_eir_type_t eir);

const char* get_avrc_metdata_attribute_name(uint8_t attributeId);
const char* get_avrc_notification_name(uint8_t eventId);
const char* get_avrc_playback_stat_name(esp_avrc_playback_stat_t playbackStat);
const char* get_avrc_battery_stat_name(esp_avrc_batt_stat_t batteryStat);
char** get_avrc_feature_names(uint32_t featureMask, char* featuresStr[6]);
char** get_avrc_feature_flags(uint16_t featureFlags, char* featuresStr[8]);

const char* get_a2d_connection_state_name(esp_a2d_connection_state_t connectionState);
const char* get_a2d_audio_state_name(esp_a2d_audio_state_t audioState);
char** get_a2d_media_codec_names(esp_a2d_mct_t codecType, char* codecNames[5]);

const char* get_a2d_sbc_sample_frequency_name(uint8_t sampleFrequency);
const char* get_a2d_sbc_channel_mode_name(uint8_t channelMode);
const char* get_a2d_sbc_block_count_name(uint8_t blockCount);
const char* get_a2d_sbc_subbands_name(uint8_t blockCount);
const char* get_a2d_sbc_allocation_mode(uint8_t allocationMode);
const char* get_a2d_init_state_name(esp_a2d_init_state_t initState);
const char* get_a2d_protocol_service_capabilities_name(esp_a2d_psc_t capabilitiesMask);
const char* get_a2d_media_command_name(esp_a2d_media_ctrl_t cmd);
const char* get_a2d_media_command_ack_name(esp_a2d_media_ctrl_ack_t ackStatus);