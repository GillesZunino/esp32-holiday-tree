// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#pragma once

#define AVRC_VOLUME_TO_PERCENT(v) ((uint16_t) (((v) * 100) / 127))
#define PERCENT_VOLUME_TO_AVRC(v) ((uint16_t) (((v) * 127) / 100))

uint8_t get_volume_avrc();
void set_volume_avrc(uint8_t volumeAvrc);

uint8_t get_volume_percent();
float get_volume_factor();