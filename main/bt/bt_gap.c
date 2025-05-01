// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_gap_bt_api.h>


#include "bt/bt_utilities.h"
#include "bt/bt_bd_addr_utils.h"
#include "bt/bt_gap.h"


// Bluetooth GAP log tag
static const char* BtGapTag = "bt_gap";


static void bt_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* rawParams);


esp_err_t setup_gap_profile() {
    // Register GAP callback - This is currently only used to receive connection status
    ESP_RETURN_ON_ERROR(esp_bt_gap_register_callback(bt_gap_callback), BtGapTag, "esp_bt_gap_register_callback() failed");
    return ESP_OK;
}

static void bt_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* rawParams) {
    const esp_bt_gap_cb_param_t* const params = (const esp_bt_gap_cb_param_t* const) rawParams;
    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (params->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
                char bdaStr[18];
                ESP_LOGI(BtGapTag, "ESP_BT_GAP_AUTH_CMPL_EVT authenticated with [%s], name '%s'", get_bda_string(params->auth_cmpl.bda, bdaStr), params->auth_cmpl.device_name);
                ESP_LOGI(BtGapTag, "ESP_BT_GAP_AUTH_CMPL_EVT link key type of current link is %s (%d)", get_gap_link_key_type_name(params->auth_cmpl.lk_type), params->auth_cmpl.lk_type);
#endif
            } else {
                ESP_LOGE(BtGapTag, "ESP_BT_GAP_AUTH_CMPL_EVT failed %d", params->auth_cmpl.stat);
            }
        }
        break;

        case ESP_BT_GAP_CONFIG_EIR_DATA_EVT:
            if (params->config_eir_data.stat == ESP_BT_STATUS_SUCCESS) {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
                ESP_LOGI(BtGapTag, "ESP_BT_GAP_CONFIG_EIR_DATA_EVT success - EIR type count %d", params->config_eir_data.eir_type_num);
                for (uint8_t typeIndex = 0; typeIndex < params->config_eir_data.eir_type_num; typeIndex++) {
                    ESP_LOGI(BtGapTag, "\tEIR_TYPE[%d]: %s (%d)", typeIndex, get_eir_name(params->config_eir_data.eir_type[typeIndex]), params->config_eir_data.eir_type[typeIndex]);
                }
#endif
            } else {
                ESP_LOGE(BtGapTag, "ESP_BT_GAP_CONFIG_EIR_DATA_EVT failed %d", params->config_eir_data.stat);
            }
        break;

        case ESP_BT_GAP_SET_AFH_CHANNELS_EVT:
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
            ESP_LOGI(BtGapTag, "ESP_BT_GAP_SET_AFH_CHANNELS_EVT: %d", params->set_afh_channels.stat);
#endif
        break;

        case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
            if (params->config_eir_data.stat == ESP_BT_STATUS_SUCCESS) {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
                char bdaStr[18];
                ESP_LOGI(BtGapTag, "ESP_BT_GAP_READ_REMOTE_NAME_EVT remote device [%s], name '%s'", get_bda_string(params->read_rmt_name.bda, bdaStr), params->read_rmt_name.rmt_name);
#endif
            } else {
                ESP_LOGE(BtGapTag, "ESP_BT_GAP_READ_REMOTE_NAME_EVT failed %d", params->read_rmt_name.stat);
            }
        }
        break;

        case ESP_BT_GAP_MODE_CHG_EVT:
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
            ESP_LOGI(BtGapTag, "ESP_BT_GAP_MODE_CHG_EVT mode %s (0x%x)", get_gap_power_mode_name(params->mode_chg.mode), params->mode_chg.mode);
#endif
        break;

        case ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT:
            if (params->remove_bond_dev_cmpl.status == ESP_BT_STATUS_SUCCESS) {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
                char bdaStr[18];
                ESP_LOGI(BtGapTag, "ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT remote device [%s] success", get_bda_string(params->remove_bond_dev_cmpl.bda, bdaStr));
#endif
            } else {
                ESP_LOGE(BtGapTag, "ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT failed %d", params->remove_bond_dev_cmpl.status);
            }
        break;

        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT: {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
            char bdaStr[18];
            ESP_LOGI(BtGapTag, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT connected to [%s], status 0x%02x", get_bda_string(params->acl_conn_cmpl_stat.bda, bdaStr), params->acl_conn_cmpl_stat.stat);
#endif
        }
        break;
        
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT: {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
            char bdaStr[18];
            ESP_LOGI(BtGapTag, "ESP_BT_GAP_ACL_DISC_CMPL_STAT_EVT disconnected from [%s], reason 0x%02x", get_bda_string(params->acl_disconn_cmpl_stat.bda, bdaStr), params->acl_disconn_cmpl_stat.reason);
#endif
        }
        break;

        case ESP_BT_GAP_ENC_CHG_EVT: {
#if CONFIG_HOLIDAYTREE_BT_GAP_LOG
            char bdaStr[18];
            ESP_LOGI(BtGapTag, "ESP_BT_GAP_ENC_CHG_EVT encryption mode to [%s] changed to %s", get_bda_string(params->enc_chg.bda, bdaStr), get_gap_encryption_mode(params->enc_chg.enc_mode));
#endif
        }
        break;

        default: {
            ESP_LOGW(BtGapTag, "bt_gap_callback() received unknown event '%d'", event);
            break;
        }
    }
}