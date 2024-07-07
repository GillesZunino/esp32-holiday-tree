// -----------------------------------------------------------------------------------
// Copyright 2024, Gilles Zunino
// -----------------------------------------------------------------------------------

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


#include "bt/bt_avrc_volume.h"


static const uint8_t startVolumePercent = 30;


static _lock_t s_volume_lock;
static uint8_t s_volume_avrc = PERCENT_VOLUME_TO_AVRC(startVolumePercent);
static uint8_t s_volume_percent = startVolumePercent;


uint8_t get_volume_avrc() {
    uint8_t volume_avrc;
    _lock_acquire(&s_volume_lock);
        volume_avrc = s_volume_avrc;
    _lock_release(&s_volume_lock);

    return volume_avrc;
}

uint8_t get_volume_percent() {
    uint8_t volume_percent;
    _lock_acquire(&s_volume_lock);
        volume_percent = s_volume_percent;
    _lock_release(&s_volume_lock);

    return volume_percent;
}

void set_volume(uint8_t volume_avrc) {
    _lock_acquire(&s_volume_lock);
        s_volume_avrc = volume_avrc;
        s_volume_percent = AVRC_VOLUME_TO_PERCENT(volume_avrc);
    _lock_release(&s_volume_lock);
}
