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
static const char* BtAvrcTargetTag = "bt_avrc_tg";



static void avrc_target_event_handler(uint16_t event, void* rawParams);



void avrc_target_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) {
    bool eventEnqueued = queue_bluetooth_workitem(avrc_target_event_handler, event, param, sizeof(esp_avrc_tg_cb_param_t));
    if (!eventEnqueued) {
        ESP_LOGE(BtAvrcTargetTag, "%s() [TG] could not queue event %d to Bluetooth dispatcher", __func__, event);
    }
}

static void avrc_target_event_handler(uint16_t event, void* rawParams) {
    const esp_avrc_tg_cb_param_t* const params = (const esp_avrc_tg_cb_param_t* const) rawParams;
    switch (event) {
        case ESP_AVRC_TG_CONNECTION_STATE_EVT: {
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
            char bdaStr[18];
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_CONNECTION_STATE_EVT %s remote [%s]",
                    params->conn_stat.connected ? "connected to" : "disconnected from", get_bda_string(params->conn_stat.remote_bda, bdaStr));
#endif
        }
        break;

        case ESP_AVRC_TG_REMOTE_FEATURES_EVT: {
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_REMOTE_FEATURES_EVT feature bit mask: 0x%"PRIx32", CT features: 0x%"PRIx16, params->rmt_feats.feat_mask, params->rmt_feats.ct_feat_flag);
            // Features
            char* featuresStr[6];
            get_avrc_feature_names(params->rmt_feats.feat_mask, featuresStr);
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_REMOTE_FEATURES_EVT rmt_feats.feat_mask (0x%"PRIx32")", params->rmt_feats.feat_mask);
            for (uint8_t index = 0; (featuresStr[index] != NULL) && (index < 6); index++) {
                ESP_LOGI(BtAvrcTargetTag, "[TG]\t%s", featuresStr[index]);
            }

            // CT Flags
            char* featureFlagsStr[8];
            get_avrc_feature_flags(params->rmt_feats.ct_feat_flag, featureFlagsStr);
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_REMOTE_FEATURES_EVT rmt_feats.ct_feat_flag (0x%"PRIx16")", params->rmt_feats.ct_feat_flag);
            for (uint8_t index = 0; (featureFlagsStr[index] != NULL) && (index < 8); index++) {
                ESP_LOGI(BtAvrcTargetTag, "[TG]\t%s", featureFlagsStr[index]);
            }
#endif
        }
        break;

        case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT: {
            // Remote controller sets the absolute volume
            uint8_t volumeAvrc = params->set_abs_vol.volume;
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
            uint16_t volume_percent = AVRC_VOLUME_TO_PERCENT(params->set_abs_vol.volume);
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT volume: %d (%d%%)", volumeAvrc, volume_percent);
#endif
            set_volume_avrc(volumeAvrc);
        }
        break;

        case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT: {
            uint8_t eventId = params->reg_ntf.event_id;
#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT -> %s (0x%x), param: 0x%"PRIx32, get_avrc_notification_name(eventId), eventId, params->reg_ntf.event_parameter);
#endif
            switch (eventId) {
                case ESP_AVRC_RN_VOLUME_CHANGE: {
                    // Respond to the controller with 'INTERIM' - See paragragh 29.19 of AVRC 1.6.1 specification ...
                    esp_avrc_rn_param_t rnParam = { .volume = get_volume_avrc() };

#if CONFIG_HOLIDAYTREE_BT_AVR_TG_LOG
            ESP_LOGI(BtAvrcTargetTag, "[TG] ESP_AVRC_RN_VOLUME_CHANGE -> INTERIM response with AVRC volume '%d'", rnParam.volume);
#endif
                    esp_err_t err = esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rnParam);
                    if (err !=ESP_OK) {
                        ESP_LOGE(BtAvrcTargetTag, "[TG] ESP_AVRC_RN_VOLUME_CHANGE -> esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM) failed (%d)", err);
                    }
                }
                break;

                default:
                    ESP_LOGW(BtAvrcTargetTag, "[TG] ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT -> Unknown Notification %s (0x%x), param: 0x%"PRIx32, get_avrc_notification_name(eventId), eventId, params->reg_ntf.event_parameter);
                break;
            }
        }
        break;

        default: {
            ESP_LOGW(BtAvrcTargetTag, "%s() [TG] unhandled event: %d", __func__, event);
        }
        break;
    }
}