// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <sys/lock.h>
#include <esp_check.h>
#include <esp_log.h>
#include <esp_avrc_api.h>


#include "bt/bt_work_dispatcher.h"
#include "bt/bt_avrc_volume.h"
#include "bt/bt_utilities.h"
#include "bt/i2s_output.h"
#include "bt/bt_avrc_target.h"



// Bluetooth AVRC Target log tags
static const char* BT_AVRC_TG_TAG = "bt_avrc_tg";



static void avrc_target_event_handler(uint16_t event, void *params);



void avrc_target_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) {
    bool eventEnqueued = queue_bluetooth_workitem(avrc_target_event_handler, event, param, sizeof(esp_avrc_tg_cb_param_t));
    if (!eventEnqueued) {
        ESP_LOGE(BT_AVRC_TG_TAG, "%s() [TG] could not queue event %d to Bluetooth dispatcher", __func__, event);
    }
}

static void avrc_target_event_handler(uint16_t event, void *params) {
    esp_avrc_tg_cb_param_t *callbackParams = (esp_avrc_tg_cb_param_t *)params;
    switch (event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT: {
            char bda_str[18];
            ESP_LOGI(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_CONNECTION_STATE_EVT %s remote [%s]",
                    callbackParams->conn_stat.connected ? "connected to" : "disconnected from", get_bda_string(callbackParams->conn_stat.remote_bda, bda_str));
        }
        break;

        case ESP_AVRC_TG_REMOTE_FEATURES_EVT: {
            ESP_LOGI(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_REMOTE_FEATURES_EVT feature bit mask: 0x%"PRIx32", CT features: 0x%"PRIx16, callbackParams->rmt_feats.feat_mask, callbackParams->rmt_feats.ct_feat_flag);
            // Features
            char* featuresStr[6];
            get_avrc_feature_names(callbackParams->rmt_feats.feat_mask, featuresStr);
            ESP_LOGI(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_REMOTE_FEATURES_EVT rmt_feats.feat_mask (0x%"PRIx32")", callbackParams->rmt_feats.feat_mask);
            for (uint8_t index = 0; (featuresStr[index] != NULL) && (index < 6); index++) {
                ESP_LOGI(BT_AVRC_TG_TAG, "[TG]\t%s", featuresStr[index]);
            }

            // CT Flags
            char* featureFlagsStr[8];
            get_avrc_feature_flags(callbackParams->rmt_feats.ct_feat_flag, featureFlagsStr);
            ESP_LOGI(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_REMOTE_FEATURES_EVT rmt_feats.ct_feat_flag (0x%"PRIx16")", callbackParams->rmt_feats.ct_feat_flag);
            for (uint8_t index = 0; (featureFlagsStr[index] != NULL) && (index < 8); index++) {
                ESP_LOGI(BT_AVRC_TG_TAG, "[TG]\t%s", featureFlagsStr[index]);
            }
        }
        break;

        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT: {
            // Remote controller sets the absolute volume
            uint8_t volume_avrc = callbackParams->set_abs_vol.volume;
            uint16_t volume_percent = AVRC_VOLUME_TO_PERCENT(callbackParams->set_abs_vol.volume);
            ESP_LOGI(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT volume: %d (%d%%)", volume_avrc, volume_percent);
            set_volume(volume_avrc);
        }
        break;

        case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT: {
            uint8_t eventId = callbackParams->reg_ntf.event_id;
            ESP_LOGI(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT -> %s (0x%x), param: 0x%"PRIx32, get_avrc_notification_name(eventId), eventId, callbackParams->reg_ntf.event_parameter);
            
            switch (eventId) {
                case ESP_AVRC_RN_VOLUME_CHANGE: {
                    uint8_t volume_avrc;
                    get_volume_avrc(&volume_avrc);
               
                    // Respond to the controller with the current AVRC volume
                    esp_avrc_rn_param_t rn_param = {
                        .volume = volume_avrc
                    };

                    // Response with 'INTERIM' as opposed to 'CHANGED' - See paragragh 292.9 of AVRC 1.6.1 specification 
                    esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rn_param);
                }
                break;

                default:
                    ESP_LOGW(BT_AVRC_TG_TAG, "[TG] ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT -> Unknown Notification %s (0x%x), param: 0x%"PRIx32, get_avrc_notification_name(eventId), eventId, callbackParams->reg_ntf.event_parameter);
                break;
            }
        }
        break;

        default: {
            ESP_LOGE(BT_AVRC_TG_TAG, "%s() [TG] unhandled event: %d", __func__, event);
        }
        break;
    }
}