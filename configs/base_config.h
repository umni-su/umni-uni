// base_config.h
// Базовый конфигурационный файл для проекта UMNI
// Все настройки имеют значения по умолчанию, которые могут быть переопределены

#pragma once

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