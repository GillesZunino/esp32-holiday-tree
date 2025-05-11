// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_a2dp_api.h>


const char* get_a2d_connection_state_name(esp_a2d_connection_state_t connectionState);
const char* get_a2d_protocol_service_capabilities_name(esp_a2d_psc_t capabilitiesMask);

const char* get_a2d_audio_state_name(esp_a2d_audio_state_t audioState);
char** get_a2d_media_codec_names(esp_a2d_mct_t codecType, char* codecNames[5]);

const char* get_a2d_sbc_sample_frequency_name(uint8_t sampleFrequency);
const char* get_a2d_sbc_channel_mode_name(uint8_t channelMode);
const char* get_a2d_sbc_block_count_name(uint8_t blockCount);
const char* get_a2d_sbc_subbands_name(uint8_t blockCount);
const char* get_a2d_sbc_allocation_mode(uint8_t allocationMode);

const char* get_a2d_init_state_name(esp_a2d_init_state_t initState);

const char* get_a2d_media_command_name(esp_a2d_media_ctrl_t cmd);
const char* get_a2d_media_command_ack_name(esp_a2d_media_ctrl_ack_t ackStatus);