// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


#include "bt/bt_avrc_volume.h"



static const uint8_t StartVolumePercent = 30;


static _lock_t s_volume_lock;
static uint8_t s_volume_avrc = PERCENT_VOLUME_TO_AVRC(StartVolumePercent);
static uint8_t s_volume_percent = StartVolumePercent;
static float s_volume_factor = 0.0f;


float get_linear_volume(uint8_t volumeAvrc);
float get_exponential_volume(uint8_t volumeAvrc);


uint8_t get_volume_avrc() {
    uint8_t volumeAvrc;
    _lock_acquire(&s_volume_lock);
        volumeAvrc = s_volume_avrc;
    _lock_release(&s_volume_lock);

    return volumeAvrc;
}

void set_volume_avrc(uint8_t volumeAvrc) {
    _lock_acquire(&s_volume_lock);
        s_volume_avrc = volumeAvrc;
        s_volume_percent = AVRC_VOLUME_TO_PERCENT(volumeAvrc);
        //
        // Pre-calculate volume. Choose between:
        //  * Linear:               get_linear_volume(volumeAvrc)
        //  * Simple Exponential:   get_exponential_volume(volumeAvrc)
        //
        s_volume_factor = get_exponential_volume(volumeAvrc);
    _lock_release(&s_volume_lock);
}

void reset_volume_avrc_to_default() {
    set_volume_avrc(PERCENT_VOLUME_TO_AVRC(StartVolumePercent));
}

uint8_t get_volume_percent() {
    uint8_t volumePercent;
    _lock_acquire(&s_volume_lock);
        volumePercent = s_volume_percent;
    _lock_release(&s_volume_lock);
    return volumePercent;
}

float get_volume_factor() {
    float volumeFactor;
    _lock_acquire(&s_volume_lock);
        volumeFactor = s_volume_factor;
    _lock_release(&s_volume_lock);
    return volumeFactor;
}

float get_linear_volume(uint8_t volumeAvrc) {
    const float VolumeMultiplier = 2.1f;
    return (VolumeMultiplier * volumeAvrc) / 127.0f;
}

float get_exponential_volume(uint8_t volumeAvrc) {
    return pow(2.0f, (float) volumeAvrc / 127.0f) - 1.0f;
}