#ifndef UM_OPENCOLLECTORS_CONFIG_H
#define UM_OPENCOLLECTORS_CONFIG_H

#include "cJSON.h"
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Конфигурация одного OC канала
    typedef struct
    {
        uint8_t channel; // 0 или 1
        char label[32];  // Имя канала
        bool active;     // Активен ли канал
    } um_oc_config_item_t;

    // Конфигурация всех OC каналов
    typedef struct
    {
        um_oc_config_item_t channels[2];
        uint8_t channel_count;
    } um_oc_config_t;

    /**
     * @brief Загружает конфигурацию из файла /spiffs/opencollectors.json
     */
    esp_err_t um_oc_config_load(void);

    /**
     * @brief Сохраняет конфигурацию в файл
     */
    esp_err_t um_oc_config_save(void);

    /**
     * @brief Создает конфиг по умолчанию
     */
    esp_err_t um_oc_config_create_default(void);

    /**
     * @brief Получить конфигурацию канала
     * @param channel 0 или 1
     */
    const um_oc_config_item_t *um_oc_config_get_channel(uint8_t channel);

    /**
     * @brief Обновить конфигурацию канала
     * @param channel 0 или 1
     * @param label Новое название (можно NULL)
     * @param active Новое состояние активности
     */
    esp_err_t um_oc_config_update_channel(uint8_t channel, const char *label, bool active);

    /**
     * @brief Получить JSON со всеми каналами и текущими состояниями
     * @return JSON строка (нужно освободить через free())
     */
    char *um_oc_config_get_json(void);

#ifdef __cplusplus
}
#endif

#endif // UM_OPENCOLLECTORS_CONFIG_H