// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_gap_bt_api.h>


const char* get_gap_link_key_type_name(esp_bt_link_key_type_t linkKeyType);
const char* get_gap_encryption_mode(esp_bt_enc_mode_t encryptionMode);
const char* get_gap_power_mode_name(esp_bt_pm_mode_t powerMode);

const char* get_eir_name(esp_bt_eir_type_t eir);