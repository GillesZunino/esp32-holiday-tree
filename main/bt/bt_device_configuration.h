// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------


#pragma once

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct bt_device_configuration {
    uint8_t volume;
} __attribute__ ((__packed__)) bt_device_configuration_t;


#ifdef __cplusplus
}
#endif