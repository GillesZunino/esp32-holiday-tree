// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <esp_check.h>
#include <esp_log.h>
#include <esp_avrc_api.h>


#include "bt/bt_avrc_volume.h"
#include "bt/bt_work_dispatcher.h"
#include "bt/bt_avrc_controller.h"
#include "bt/bt_avrc_target.h"
#include "bt/bt_avrc.h"



// Bluetooth AVRC log tag
static const char* BtAvrcTag = "bt_avrc";


static esp_err_t register_target_notifications_capabilities();


esp_err_t setup_avrc_profile() {
    //
    // In AVRC parlance, there are two roles: Controller (CT) and Target (TG):
    //
    // * Controller is typically a remote control which can ask the target to perform operations like play, pause ...
    //   Hadphones can also be a Controller if they have built in buttons for next, previous, skip ...
    // * Target is the device being controlled, for instance a media player, a tuner...
    // 
    // When AVRC is used in collaboration with A2DP, the roles are typically as follows:
    // * A2DP Source (Phone, Computer ...) encodes audio and sends it to ...
    // * A2DP Sink (Headphone, Wireless speaker ...) decodes audio and plays it
    //
    // WHen A2DP and AVRC are used toegether, roles are typically assigned as follows:
    // * AVRC CT is typically associated with A2DP source
    // * AVRC TG is typically associated with A2DP sink
    //
    //
    // The following summerizes various common arrangements:
    //
    //  * A2DP Source | AVRC Controller  -> A2DP Sink | AVRC Target:
    //      A Phone, Computer ... produces Audio and sends it to a wireless speaker with a single volume button
    //      AVRC TG (wireless speaker) can inform AVRC CT (Phone, Computer ...) of volume change on the wireless speaker
    //
    //  * A2DP Source | AVRC Controller -> A2DP Sink | AVRC Target | AVRC Controller:
    //      A Phone, Computer ... produces Audio and sends it to a wireless speaker with a control panel (play, pause, skip, now playing ...)
    //      AVRC TG (wireless speaker) can inform AVRC CT (Phone, Computer ...) of volume change on the wireless speaker
    //      AVRC CT (wireless speaker) can 'remote control' the Phone, Computer... via AVRC when user triggers a control panel function
    //
    // !This code uses the "A2DP Source | AVRC Controller -> A2DP Sink | AVRC Target | AVRC Controller" approach
    // This is mostly because we would like to get metadata about the track playing and AVRC TG does not allow metadata exchange
    // To receive metadata, the Holiday Tree needs to be an AVRC Controller and subscribe for target notificaitons
    //

    // Ensure output volume is set to default
    set_volume_avrc(get_default_volume_avrc());

    // Initialize AVRC Controller module
    ESP_RETURN_ON_ERROR(esp_avrc_ct_init(), BtAvrcTag, "esp_avrc_ct_init() failed");

    // Register AVRC Controller callback
    ESP_RETURN_ON_ERROR(esp_avrc_ct_register_callback(avrc_controller_callback), BtAvrcTag, "esp_avrc_ct_register_callback() failed");

    // Initialize AVRC Target module
    ESP_RETURN_ON_ERROR(esp_avrc_tg_init(), BtAvrcTag, "esp_avrc_tg_init() failed");

    // Register AVRC Target callback
    ESP_RETURN_ON_ERROR(esp_avrc_tg_register_callback(avrc_target_callback), BtAvrcTag, "esp_avrc_tg_register_callback() failed");

    // Register Target notification capabilities so the controller can request information from this device
    ESP_RETURN_ON_ERROR(register_target_notifications_capabilities(), BtAvrcTag, "register_target_notifications_capabilities() failed");
    return ESP_OK;
}

static esp_err_t register_target_notifications_capabilities() {
    esp_err_t err = ESP_FAIL;

    // On the Holiday Tree, configure AVRC TG to tell the Phone, Computer ... AVRC CT that the Holiday Tree can notify of 'VOLUME_CHANGE'
    esp_avrc_rn_evt_cap_mask_t evtSet = {0};
    bool bitSetOutcome = esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evtSet, ESP_AVRC_RN_VOLUME_CHANGE);
    if (bitSetOutcome) {
        err = esp_avrc_tg_set_rn_evt_cap(&evtSet);
    }
    return err;
}