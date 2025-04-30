// -----------------------------------------------------------------------------------
// Copyright 2025, Gilles Zunino
// -----------------------------------------------------------------------------------


#pragma once

#include <esp_check.h>
#include <esp_avrc_api.h>


esp_err_t bt_device_manager_device_connected(const struct avrc_tg_conn_stat_param* const params);
esp_err_t bt_device_manager_device_disconnected(const struct avrc_tg_conn_stat_param* const params);