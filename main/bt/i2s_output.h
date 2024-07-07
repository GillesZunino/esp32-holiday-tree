// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#include <esp_err.h>
#include <driver/i2s_std.h>


esp_err_t create_i2s_output();
esp_err_t start_i2s_output();
esp_err_t delete_i2s_output();

esp_err_t configure_i2s_output(uint32_t sampleRate, i2s_data_bit_width_t dataWidth, i2s_slot_mode_t slotMode);
uint32_t write_to_i2s_output(const uint8_t* data, uint32_t size);
