// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_bt_defs.h>


bool is_null_bda(const uint8_t* const bda);

void copy_bda(uint8_t* dest_bda, const uint8_t* const src_bda);
void clear_bda(uint8_t* bda);

const char* get_bda_string(const esp_bd_addr_t bda, char str[18]);