// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------


#pragma once

#include <esp_err.h>

#include <nvs.h>


esp_err_t nvs_get_configuration(const char namespace[NVS_NS_NAME_MAX_SIZE], const char key[NVS_KEY_NAME_MAX_SIZE], void* data, size_t* dataSize);
esp_err_t nvs_set_configuration(const char namespace[NVS_NS_NAME_MAX_SIZE], const char key[NVS_KEY_NAME_MAX_SIZE], const void* const data, const size_t dataSize);
