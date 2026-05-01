#ifndef UM_OPENTHERM_H
#define UM_OPENTHERM_H

#pragma once
#include <stdio.h>
#include <inttypes.h>
#include "esp_err.h"
#include "opentherm.h"

typedef struct
{
    bool modulation;       // Поддерживает ли котел чтение модуляции
    bool modulation_write; // Можно ли задавать макс. модуляцию
    bool heat_curve;       // Поддерживает ли кривую нагрева
    bool heat_curve_write; // Можно ли задавать кривую
    bool dhw_setpoint;     // Поддерживает ли уставку DHW
    bool ch_setpoint;      // Поддерживает ли уставку CH
    bool outside_temp;     // Есть ли датчик наружной температуры
    bool return_temp;      // Есть ли датчик обратки
    bool pressure;         // Есть ли датчик давления
    bool flow_rate;        // Есть ли расходомер DHW
} um_ot_supported_t;

typedef struct
{
    bool adapter_success;
    bool ready; // data ready or not

    bool ch_en; // from NVS
    int ch_sp;  // from NVS

    bool ch2_en; // from NVS

    bool dhw_en; // from NVS
    int dhw_sp;  // from NVS

    bool otc_en; // from NVS
    int hcr;     // from NVS

    int status;

    bool central_heating_active;
    bool hot_water_active;
    bool flame_on;

    float modulation;
    bool is_fault;
    int fault_code;
    float return_temperature;
    float dhw_temperature;
    float boiler_temperature;
    float pressure;
    unsigned long slave_product_version;
    float slave_ot_version;
    //float ch_max_setpoint;
    float dhw_setpoint;
    float outside_temperature;
    float flow_rate;
    float heat_curve_ratio;
    float flow_rate_ch2;
    int mod; // modulation level setting
    esp_ot_min_max_t dhw_min_max;
    esp_ot_min_max_t ch_min_max;
    esp_ot_min_max_t curve_bounds;
    esp_ot_cap_mod_t cap_mod;
    esp_ot_asf_flags_t asf_flags;
    esp_ot_slave_config_t slave_config;
    um_ot_supported_t supported;

} um_ot_data_t;

um_ot_data_t um_ot_get_data();

void um_opentherm_control_task_handler(void *pvParameter);

void um_ot_init();

esp_err_t um_ot_set_boiler_status(
    bool enable_central_heating,
    bool enable_hot_water,
    bool enable_cooling,
    bool enable_outside_temperature_compensation,
    bool enable_central_heating2);

void um_ot_set_boiler_temp(float temp);

void um_ot_set_dhw_setpoint(float temp);

void um_ot_update_state(bool otch, int otdhw, int ottbsp);

void um_ot_set_central_heating_active(bool state);

void um_ot_set_hot_water_active(bool state);

void um_ot_reset_error();

void um_ot_set_ch2(bool active);

void um_ot_set_outside_temp_comp(bool state);

void um_ot_set_modulation_level(int level);

void um_ot_set_hot_water_active(bool hwa);

void um_ot_set_heat_curve_ratio(int ratio);

char *um_ot_get_status_json(void);

void um_ot_detect_supported_features(void);

#endif