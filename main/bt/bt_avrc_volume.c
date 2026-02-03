// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


#include "bt/bt_avrc_volume.h"



// -----------------------------------------------------------------------------------
// There are many ways to calculate the multiplier factor applied to all sound samples
// Choose between:
//  * Linear:               get_linear_volume(volumeAvrc)
//  * Simple Exponential:   get_exponential_volume(volumeAvrc)
//  * dB Curve:             get_dB_volume(volumeAvrc)

[[maybe_unused]] static float get_linear_volume(uint8_t volumeAvrc);
[[maybe_unused]] static float get_exponential_volume(uint8_t volumeAvrc);
[[maybe_unused]] static float get_dB_volume(uint8_t volumeAvrc);

// Define the function to use in the macro below
#define AVRC_VOLUME_TO_FACTOR(x) get_exponential_volume(x)
// -----------------------------------------------------------------------------------

// Maximum AVRC volume value
static const uint8_t MaxAVRCVolume = 127;

// Default volume at startup (30% of max)
static const uint8_t DefaultVolumeAvrc = PERCENT_VOLUME_TO_AVRC(30);


static _lock_t s_volume_lock;
static uint8_t s_volume_avrc = 0;
static uint8_t s_volume_percent = 0;
static float s_volume_factor = 0.0f;



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
        //  * dB Curve:             get_dB_volume(volumeAvrc)
        //
        s_volume_factor = AVRC_VOLUME_TO_FACTOR(volumeAvrc);
    _lock_release(&s_volume_lock);
}

uint8_t get_default_volume_avrc() {
    return DefaultVolumeAvrc;
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

[[maybe_unused]] static float get_linear_volume(uint8_t volumeAvrc) {
    const float VolumeMultiplier = 2.1f;
    return (VolumeMultiplier * volumeAvrc) / (float) MaxAVRCVolume;
}

[[maybe_unused]] static float get_exponential_volume(uint8_t volumeAvrc) {
    return pow(2.0f, (float) volumeAvrc / (float) MaxAVRCVolume) - 1.0f;
}

[[maybe_unused]] static float get_dB_volume(uint8_t volumeAvrc) {
    const float MinVolumeDb = -25.0f;  // Quiet end (mute-ish)
    const float MaxVolumeDb = 0.5f;    // Slightly above Unity gain (0dB) at max step
    if (volumeAvrc == 0) {
        return 0.0f;
    } else {
        float volumeFactor = (float) volumeAvrc / (float) MaxAVRCVolume;
        float targetDb = MinVolumeDb + volumeFactor * (MaxVolumeDb - MinVolumeDb);
        return powf(10.0f, targetDb / 20.0f);
    }
}