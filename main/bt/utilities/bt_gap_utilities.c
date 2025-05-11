// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#include "bt/utilities/bt_gap_utilities.h"


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