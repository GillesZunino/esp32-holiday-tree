// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_avrc_api.h>


#include "bt/bt_work_dispatcher.h"
#include "bt/bt_avrc_volume.h"
#include "bt/bt_utilities.h"
#include "bt/bt_avrc_controller.h"



// Bluetooth AVRC Controller log tags
static const char* BtAvrcControllerTag = "bt_avrc_ct";


// Commands Transaction Labels = These must be the same across notifications - There is a maximum of 15 transaction labels
static const uint8_t CommandGetCapabilitiesTransactionLabel = 0;
static const uint8_t COmmandGetMetadataTransactionLabel = 1;

// Notification Transaction Labels = These must be the same across notifications - There is a maximum of 15 transaction labels
static const uint8_t NotificationTrackChangeTransactionLabel = 2;
static const uint8_t NotificationPlaybackChangeTransactionLabel = 3;
static const uint8_t NotificationPlayPositionChangedTransactionLabel = 4;


// Stores notification capabilities of the Controller sending us audio via A2DP
static esp_avrc_rn_evt_cap_mask_t s_avrc_peer_notifications_capabilities = {0};


static void avrc_controller_event_handler(uint16_t event, void* param);
static esp_err_t handle_controller_notification_event(uint8_t event, esp_avrc_rn_param_t *params);
static esp_err_t register_for_notification(esp_avrc_rn_event_ids_t event_to_register_for, uint8_t transactionLabel, uint32_t eventParameter);


void avrc_controller_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) {
    switch (event) {
        case ESP_AVRC_CT_METADATA_RSP_EVT: {
            // Copy metadata strings before enqueing the event
            uint8_t* attrText = (uint8_t *) malloc(param->meta_rsp.attr_length + 1);
            if (attrText != NULL) {
                memcpy(attrText, param->meta_rsp.attr_text, param->meta_rsp.attr_length);
                attrText[param->meta_rsp.attr_length] = 0;

                // Save the current metadata string
                uint8_t* original_attrText = param->meta_rsp.attr_text;
                param->meta_rsp.attr_text = attrText;

                // Queue event for processing - Free allocated metadata on failure - The callback calls free() on allocated memory
                bool eventQueued = queue_bluetooth_workitem(avrc_controller_event_handler, event, param, sizeof(esp_avrc_ct_cb_param_t));
                if (!eventQueued) {
                    free(param->meta_rsp.attr_text);
                }

                // Restore metadata string
                param->meta_rsp.attr_text = original_attrText;
            } else {
                ESP_LOGE(BtAvrcControllerTag, "%s() [CT] ESP_AVRC_CT_METADATA_RSP_EVT could not be handled. Failed to allocate: %d", __func__, param->meta_rsp.attr_length);
            }
        }
        break;

        default: {
            bool eventQueued = queue_bluetooth_workitem(avrc_controller_event_handler, event, param, sizeof(esp_avrc_ct_cb_param_t));
            if (!eventQueued) {
                ESP_LOGE(BtAvrcControllerTag, "%s() [CT] could not queue event %d to Bluetooth dispatcher", __func__, event);
            }
        }
        break;
    }
}

static void avrc_controller_event_handler(uint16_t event, void* param) {
    esp_avrc_ct_cb_param_t* callbackParams = (esp_avrc_ct_cb_param_t *) param;
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
            char bdaStr[18];
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_CONNECTION_STATE_EVT %s remote [%s]",
                    callbackParams->conn_stat.connected ? "connected to" : "disconnected from", get_bda_string(callbackParams->conn_stat.remote_bda, bdaStr));
            
            // When connected, retrieve remote AVRC TG supported notification events so we can subscribe to notifications (play, pause ...)
            s_avrc_peer_notifications_capabilities.bits = 0;
            if (callbackParams->conn_stat.connected) {
                if (esp_avrc_ct_send_get_rn_capabilities_cmd(CommandGetCapabilitiesTransactionLabel) != ESP_OK) {
                    ESP_LOGE(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_CONNECTION_STATE_EVT failed in esp_avrc_ct_send_get_rn_capabilities_cmd()");
                }
            }
        }
        break;

        case ESP_AVRC_CT_METADATA_RSP_EVT: {
            uint8_t attributeId = callbackParams->meta_rsp.attr_id;
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_METADATA_RSP_EVT -> %s (0x%x): '%s'", get_avrc_metdata_attribute_name(attributeId), attributeId , callbackParams->meta_rsp.attr_text);
            if (callbackParams->meta_rsp.attr_text != NULL) {
                // Free memory we allocated for the metadata string
                free(callbackParams->meta_rsp.attr_text);
                callbackParams->meta_rsp.attr_text = NULL;
            }
        }
        break;

        case ESP_AVRC_CT_PLAY_STATUS_RSP_EVT: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_PLAY_STATUS_RSP_EVT");
        }
        break;

       case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_CHANGE_NOTIFY_EVT -> %s (0x%x)", get_avrc_notification_name(callbackParams->change_ntf.event_id), callbackParams->change_ntf.event_id);
            if (handle_controller_notification_event(callbackParams->change_ntf.event_id, &callbackParams->change_ntf.event_parameter) != ESP_OK) {
                ESP_LOGE(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_CHANGE_NOTIFY_EVT failed in handle_controller_notification_event()");
            }
        }
        break;

        case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_REMOTE_FEATURES_EVT feature bit mask: 0x%"PRIx32", TG features: 0x%"PRIx16, callbackParams->rmt_feats.feat_mask, callbackParams->rmt_feats.tg_feat_flag);
            
            // Features
            char* featuresStr[6];
            get_avrc_feature_names(callbackParams->rmt_feats.feat_mask, featuresStr);
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_REMOTE_FEATURES_EVT rmt_feats.feat_mask (0x%"PRIx32")", callbackParams->rmt_feats.feat_mask);
            for (uint8_t index = 0; (featuresStr[index] != NULL) && (index < 6); index++) {
                ESP_LOGI(BtAvrcControllerTag, "[CT]\t%s", featuresStr[index]);
            }

            // CT Flags
            char* featureFlagsStr[8];
            get_avrc_feature_flags(callbackParams->rmt_feats.tg_feat_flag, featureFlagsStr);
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_REMOTE_FEATURES_EVT rmt_feats.tg_feat_flag (0x%"PRIx16")", callbackParams->rmt_feats.tg_feat_flag);
            for (uint8_t index = 0; (featureFlagsStr[index] != NULL) && (index < 8); index++) {
                ESP_LOGI(BtAvrcControllerTag, "[CT]\t%s", featureFlagsStr[index]);
            }
        }
        break;

        case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT remote rn_cap: count %d, bitmask 0x%x", callbackParams->get_rn_caps_rsp.cap_count, callbackParams->get_rn_caps_rsp.evt_set.bits);

            // Capture remote controller capabilities - We will need them when a notification is received and we need to re-attach the event notification
            s_avrc_peer_notifications_capabilities.bits = callbackParams->get_rn_caps_rsp.evt_set.bits;
            
            // We now have the remote controller capabilities. Register for notifications we need and are supported
            register_for_notification(ESP_AVRC_RN_TRACK_CHANGE, NotificationTrackChangeTransactionLabel, 0);
            register_for_notification(ESP_AVRC_RN_PLAY_STATUS_CHANGE, NotificationPlaybackChangeTransactionLabel, 0);
            register_for_notification(ESP_AVRC_RN_PLAY_POS_CHANGED, NotificationPlayPositionChangedTransactionLabel, 10);
        }
        break;

        case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT: {
            uint8_t volumeAvrc = callbackParams->set_volume_rsp.volume;
            uint16_t volumePercent = AVRC_VOLUME_TO_PERCENT(volumeAvrc);
            ESP_LOGI(BtAvrcControllerTag, "[CT] ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT volume: %d (%d%%)", volumeAvrc, volumePercent);
        }
        break;

        default:
            ESP_LOGI(BtAvrcControllerTag, "%s() [CT] received unknown event: %d", __func__, event);
            break;
    }
}

static esp_err_t handle_controller_notification_event(uint8_t event, esp_avrc_rn_param_t *params) {
    switch (event) {
        case ESP_AVRC_RN_PLAY_STATUS_CHANGE: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] [NOTIFY] ESP_AVRC_RN_PLAY_STATUS_CHANGE: Playback status changed: 0x%x", params->playback);
            return register_for_notification(ESP_AVRC_RN_PLAY_STATUS_CHANGE, NotificationPlaybackChangeTransactionLabel, 0);
        }

        case ESP_AVRC_RN_TRACK_CHANGE: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] [NOTIFY] ESP_AVRC_RN_TRACK_CHANGE");
            esp_err_t send_err = esp_avrc_ct_send_metadata_cmd(COmmandGetMetadataTransactionLabel, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM | ESP_AVRC_MD_ATTR_GENRE);
            esp_err_t register_err = register_for_notification(ESP_AVRC_RN_TRACK_CHANGE, NotificationTrackChangeTransactionLabel, 0);
            return send_err == ESP_OK ? register_err : send_err;
        }

        case ESP_AVRC_RN_PLAY_POS_CHANGED: {
            ESP_LOGI(BtAvrcControllerTag, "[CT] [NOTIFY] ESP_AVRC_RN_PLAY_POS_CHANGED: Play position changed: %"PRIu32"-ms", params->play_pos);
            return register_for_notification(ESP_AVRC_RN_PLAY_POS_CHANGED, NotificationPlayPositionChangedTransactionLabel, 10);
        }

        default: {
            ESP_LOGW(BtAvrcControllerTag, "[CT] [NOTIFY] unhandled event: %d", event);
            return ESP_OK;
        }
    }
}

static esp_err_t register_for_notification(esp_avrc_rn_event_ids_t eventToRegisterFor, uint8_t transactionLabel, uint32_t eventParameter) {
    if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &s_avrc_peer_notifications_capabilities, eventToRegisterFor)) {
        return esp_avrc_ct_send_register_notification_cmd(transactionLabel, eventToRegisterFor, eventParameter);
    }

    return ESP_OK;
}