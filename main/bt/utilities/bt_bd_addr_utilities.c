// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>

#include "bt/utilities/bt_bd_addr_utilities.h"


bool is_null_bda(const uint8_t* const bda) {
    for (uint8_t index = 0; index < ESP_BD_ADDR_LEN; index++) {
        if (bda[index] != 0) {
            return false;
        }
    }
    return true;
}

void copy_bda(uint8_t* dest_bda, const uint8_t* const src_bda) {
    memcpy(dest_bda, src_bda, ESP_BD_ADDR_LEN);
}

void clear_bda(uint8_t* bda) {
    memset(bda, 0, ESP_BD_ADDR_LEN);
}

const char* get_bda_string(const esp_bd_addr_t bda, char str[18]) {
    if ((bda == NULL) || (str == NULL)) {
        return NULL;
    }

    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}