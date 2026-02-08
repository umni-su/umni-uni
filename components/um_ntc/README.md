```c
#include "um_ntc.h"

// Конфигурация для двух каналов NTC
static um_ntc_channel_t ntc_channel1;
static um_ntc_channel_t ntc_channel2;

void init_ntc_channels(void)
{
    // Конфигурация первого канала
    um_ntc_config_t config1 = {
        .channel = ADC_CHANNEL_3,
        .unit = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 10000,
        .vdd_mv = 3300,
        .circuit = CIRCUIT_MODE_NTC_GND
    };

    // Конфигурация второго канала
    um_ntc_config_t config2 = {
        .channel = ADC_CHANNEL_4,
        .unit = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .b_value = 3950,
        .r25_ohm = 10000,
        .fixed_ohm = 10000,
        .vdd_mv = 3300,
        .circuit = CIRCUIT_MODE_NTC_GND
    };

    // Инициализация каналов
    um_ntc_channel_init(&ntc_channel1, &config1);
    um_ntc_channel_init(&ntc_channel2, &config2);

    // Включение каналов
    um_ntc_set_enabled(&ntc_channel1, true);
    um_ntc_set_enabled(&ntc_channel2, true);
}

void read_temperatures(void)
{
    float temp1, temp2;
    
    ret = um_ntc_read_temperature(UM_NTC_CHANNEL_1, temp1);
    if (ret == ESP_OK)
    {
        success_mask |= 0x01;
    }
}
```