// base_config.h
// Базовый конфигурационный файл для проекта UMNI
// Все настройки имеют значения по умолчанию, которые могут быть переопределены

#pragma once

// ============================================
// Группа 1: Версия прошивки
// ============================================
#ifndef CONFIG_UMNI_FW_VERSION
#define CONFIG_UMNI_FW_VERSION "1.0.0"
#endif

// ============================================
// Группа 2: Основные сетевые интерфейсы
// ============================================
#ifndef CONFIG_UM_FEATURE_ETHERNET
#define CONFIG_UM_FEATURE_ETHERNET 0
#endif

#ifndef CONFIG_UM_FEATURE_WIFI
#define CONFIG_UM_FEATURE_WIFI 0
#endif

// ============================================
// Группа 3: Периферия и хранилище
// ============================================
#ifndef CONFIG_UM_FEATURE_SDCARD
#define CONFIG_UM_FEATURE_SDCARD 0
#endif

// ============================================
// Группа 4: Сетевые сервисы и протоколы
// ============================================
#ifndef CONFIG_UM_FEATURE_WEBSERVER
#define CONFIG_UM_FEATURE_WEBSERVER 0
#endif

#ifndef CONFIG_UM_FEATURE_WEBHOOKS
#define CONFIG_UM_FEATURE_WEBHOOKS 0
#endif

#ifndef CONFIG_UM_FEATURE_MQTT
#define CONFIG_UM_FEATURE_MQTT 0
#endif

// ============================================
// Группа 5: Промышленные протоколы связи
// ============================================
#ifndef CONFIG_UM_FEATURE_OPENTHERM
#define CONFIG_UM_FEATURE_OPENTHERM 0
#endif

#ifndef CONFIG_UM_FEATURE_RF433
#define CONFIG_UM_FEATURE_RF433 0
#endif

// ============================================
// Группа 6: Шина 1-Wire и датчики
// ============================================
#ifndef CONFIG_UM_FEATURE_ONEWIRE
#define CONFIG_UM_FEATURE_ONEWIRE 0
#endif

#ifndef CONFIG_UM_FEATURE_ALARM
#define CONFIG_UM_FEATURE_ALARM 0
#endif

// ============================================
// Группа 7: Аналоговые входы (ADC)
// ============================================
#ifndef CONFIG_UM_FEATURE_ADC
#define CONFIG_UM_FEATURE_ADC 0
#endif

#ifndef CONFIG_UM_FEATURE_NTC1
#define CONFIG_UM_FEATURE_NTC1 0
#endif

#ifndef CONFIG_UM_FEATURE_NTC2
#define CONFIG_UM_FEATURE_NTC2 0
#endif

#ifndef CONFIG_UM_FEATURE_AI1
#define CONFIG_UM_FEATURE_AI1 0
#endif

#ifndef CONFIG_UM_FEATURE_AI2
#define CONFIG_UM_FEATURE_AI2 0
#endif

// ============================================
// Группа 8: Открытые коллекторы (выходы)
// ============================================
#ifndef CONFIG_UM_FEATURE_OPENCOLLECTORS
#define CONFIG_UM_FEATURE_OPENCOLLECTORS 0
#endif

#ifndef CONFIG_UM_FEATURE_OC1
#define CONFIG_UM_FEATURE_OC1 0
#endif

#ifndef CONFIG_UM_FEATURE_OC2
#define CONFIG_UM_FEATURE_OC2 0
#endif

// ============================================
// Группа 9: Зуммер
// ============================================
#ifndef CONFIG_UM_FEATURE_BUZZER
#define CONFIG_UM_FEATURE_BUZZER 0
#endif

// ============================================
// Группа 10: Цифровые входы
// ============================================
#ifndef CONFIG_UM_FEATURE_INPUTS
#define CONFIG_UM_FEATURE_INPUTS 0
#endif

#ifndef CONFIG_UM_FEATURE_INP1
#define CONFIG_UM_FEATURE_INP1 0
#endif

#ifndef CONFIG_UM_FEATURE_INP2
#define CONFIG_UM_FEATURE_INP2 0
#endif

#ifndef CONFIG_UM_FEATURE_INP3
#define CONFIG_UM_FEATURE_INP3 0
#endif

#ifndef CONFIG_UM_FEATURE_INP4
#define CONFIG_UM_FEATURE_INP4 0
#endif

#ifndef CONFIG_UM_FEATURE_INP5
#define CONFIG_UM_FEATURE_INP5 0
#endif

#ifndef CONFIG_UM_FEATURE_INP6
#define CONFIG_UM_FEATURE_INP6 0
#endif

// ============================================
// Группа 11: Цифровые выходы
// ============================================
#ifndef CONFIG_UM_FEATURE_OUTPUTS
#define CONFIG_UM_FEATURE_OUTPUTS 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT1
#define CONFIG_UM_FEATURE_OUT1 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT2
#define CONFIG_UM_FEATURE_OUT2 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT3
#define CONFIG_UM_FEATURE_OUT3 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT4
#define CONFIG_UM_FEATURE_OUT4 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT5
#define CONFIG_UM_FEATURE_OUT5 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT6
#define CONFIG_UM_FEATURE_OUT6 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT7
#define CONFIG_UM_FEATURE_OUT7 0
#endif

#ifndef CONFIG_UM_FEATURE_OUT8
#define CONFIG_UM_FEATURE_OUT8 0
#endif

// ============================================
// Группа 12: Настройки GPIO (Ethernet SPI)
// ============================================
#ifndef CONFIG_ETHERNET_SPI_MISO_GPIO
#define CONFIG_ETHERNET_SPI_MISO_GPIO 19
#endif

#ifndef CONFIG_ETHERNET_SPI_MOSI_GPIO
#define CONFIG_ETHERNET_SPI_MOSI_GPIO 21
#endif

#ifndef CONFIG_ETHERNET_SPI_SCLK_GPIO
#define CONFIG_ETHERNET_SPI_SCLK_GPIO 18
#endif

#ifndef CONFIG_ETH_SPI_ETHERNET_CS_GPIO
#define CONFIG_ETH_SPI_ETHERNET_CS_GPIO 5
#endif

#ifndef CONFIG_ETH_SPI_INTR_GPIO
#define CONFIG_ETH_SPI_INTR_GPIO 4
#endif

// ============================================
// Группа 13: Настройки GPIO (SD Card)
// ============================================
#ifndef CONFIG_SDSPI_CS_GPIO
#define CONFIG_SDSPI_CS_GPIO 32
#endif

#ifndef CONFIG_SDSPI_DETECT_GPIO
#define CONFIG_SDSPI_DETECT_GPIO 33
#endif

// ============================================
// Группа 14: Настройки GPIO (I2C)
// ============================================
#ifndef CONFIG_I2C_MASTER_SDA_GPIO
#define CONFIG_I2C_MASTER_SDA_GPIO 23
#endif

#ifndef CONFIG_I2C_MASTER_SCL_GPIO
#define CONFIG_I2C_MASTER_SCL_GPIO 22
#endif

// ============================================
// Группа 15: Настройки GPIO (Промышленные протоколы)
// ============================================
#ifndef CONFIG_UM_CFG_OT_IN_GPIO
#define CONFIG_UM_CFG_OT_IN_GPIO 26
#endif

#ifndef CONFIG_UM_CFG_OT_OUT_GPIO
#define CONFIG_UM_CFG_OT_OUT_GPIO 25
#endif

#ifndef CONFIG_UM_CFG_RF433_DATA_GPIO
#define CONFIG_UM_CFG_RF433_DATA_GPIO 27
#endif

// ============================================
// Группа 16: Настройки GPIO (Датчики и периферия)
// ============================================
#ifndef CONFIG_UM_CFG_ONEWIRE_GPIO
#define CONFIG_UM_CFG_ONEWIRE_GPIO 17
#endif

#ifndef CONFIG_UM_CFG_ALARM_GPIO
#define CONFIG_UM_CFG_ALARM_GPIO 13
#endif

// ============================================
// Группа 17: Настройки ADC каналов
// ============================================
#ifndef CONFIG_UM_CFG_NTC1_ADC_CHANNEL
#define CONFIG_UM_CFG_NTC1_ADC_CHANNEL 36
#endif

#ifndef CONFIG_UM_CFG_NTC2_ADC_CHANNEL
#define CONFIG_UM_CFG_NTC2_ADC_CHANNEL 39
#endif

#ifndef CONFIG_UM_CFG_AI1_ADC_CHANNEL
#define CONFIG_UM_CFG_AI1_ADC_CHANNEL 34
#endif

#ifndef CONFIG_UM_CFG_AI2_ADC_CHANNEL
#define CONFIG_UM_CFG_AI2_ADC_CHANNEL 35
#endif

// ============================================
// Группа 18: Настройки GPIO (Выходные устройства)
// ============================================
#ifndef CONFIG_UM_CFG_OC1_GPIO
#define CONFIG_UM_CFG_OC1_GPIO 14
#endif

#ifndef CONFIG_UM_CFG_OC2_GPIO
#define CONFIG_UM_CFG_OC2_GPIO 12
#endif

#ifndef CONFIG_UM_CFG_BUZZER_GPIO
#define CONFIG_UM_CFG_BUZZER_GPIO 15
#endif

// ============================================
// Группа 19: Индексы входов
// ============================================
#ifndef CONFIG_UM_CFG_INP1_INDEX
#define CONFIG_UM_CFG_INP1_INDEX 1
#endif

#ifndef CONFIG_UM_CFG_INP2_INDEX
#define CONFIG_UM_CFG_INP2_INDEX 2
#endif

#ifndef CONFIG_UM_CFG_INP3_INDEX
#define CONFIG_UM_CFG_INP3_INDEX 3
#endif

#ifndef CONFIG_UM_CFG_INP4_INDEX
#define CONFIG_UM_CFG_INP4_INDEX 4
#endif

#ifndef CONFIG_UM_CFG_INP5_INDEX
#define CONFIG_UM_CFG_INP5_INDEX 5
#endif

#ifndef CONFIG_UM_CFG_INP6_INDEX
#define CONFIG_UM_CFG_INP6_INDEX 6
#endif

// ============================================
// Группа 20: Индексы выходов
// ============================================
#ifndef CONFIG_UM_CFG_OUT1_INDEX
#define CONFIG_UM_CFG_OUT1_INDEX 1
#endif

#ifndef CONFIG_UM_CFG_OUT2_INDEX
#define CONFIG_UM_CFG_OUT2_INDEX 2
#endif

#ifndef CONFIG_UM_CFG_OUT3_INDEX
#define CONFIG_UM_CFG_OUT3_INDEX 3
#endif

#ifndef CONFIG_UM_CFG_OUT4_INDEX
#define CONFIG_UM_CFG_OUT4_INDEX 4
#endif

#ifndef CONFIG_UM_CFG_OUT5_INDEX
#define CONFIG_UM_CFG_OUT5_INDEX 5
#endif

#ifndef CONFIG_UM_CFG_OUT6_INDEX
#define CONFIG_UM_CFG_OUT6_INDEX 6
#endif

#ifndef CONFIG_UM_CFG_OUT7_INDEX
#define CONFIG_UM_CFG_OUT7_INDEX 7
#endif

#ifndef CONFIG_UM_CFG_OUT8_INDEX
#define CONFIG_UM_CFG_OUT8_INDEX 8
#endif

// ============================================
// Удобные макросы для проверки включения фич
// ============================================
#define UM_FEATURE_ENABLED(feature) (CONFIG_UM_FEATURE_##feature == 1)

// Пример использования:
// #if UM_FEATURE_ENABLED(UM_FEATURE_ETHERNET)
// // код для работы с Ethernet
// #endif

// Макрос для проверки отключения фичи
#define UM_FEATURE_DISABLED(feature) (CONFIG_UM_FEATURE_##feature == 0)

// Макрос для условной компиляции с ветвлением else
#define IF_FEATURE_ENABLED(feature, code_if_enabled, code_if_disabled) \
    do { \
        if (CONFIG_##feature == 1) { \
            code_if_enabled \
        } else { \
            code_if_disabled \
        } \
    } while(0)