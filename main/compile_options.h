// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

// Static string listing all compile options
const char* CompileTimeOptions =
    #if CONFIG_HOLIDAYTREE_HARDWARE_PRODUCTION
        "HARDWARE_PRODUCTION"
    #else
        "HARDWARE_DEVELOPMENT"
    #endif
    #if CONFIG_HOLIDAYTREE_DETAILED_I2S_DATA_PROCESSING_LOG
        "|I2S LOGS"
    #endif
    "|BR_EDR_DEVICE_NAME_STR:" CONFIG_HOLIDAYTREE_BR_EDR_DEVICE_NAME_STR ""

    #if CONFIG_HOLIDAYTREE_BR_EDR_LEGACY_PAIRING_REQUIRE_STATIC_PIN
        #if CONFIG_HOLIDAYTREE_HARDWARE_PRODUCTION
            #if CONFIG_OPTIMIZATION_LEVEL_DEBUG
                "|PAIRING_REQUIRE_STATIC_PIN:" CONFIG_HOLIDAYTREE_BR_EDR_STATIC_PIN_STR
            #else
                "|PAIRING_REQUIRE_STATIC_PIN <HIDDEN>"
            #endif
        #else
            #if CONFIG_HOLIDAYTREE_HARDWARE_DEVELOPMENT
                "|PAIRING_REQUIRE_STATIC_PIN:" CONFIG_HOLIDAYTREE_BR_EDR_STATIC_PIN_STR
            #endif
        #endif
    #else
        "|PAIRING_REQUIRE_STATIC_PIN OFF"
    #endif
;