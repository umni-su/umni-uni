#ifndef UM_RF433_CONFIG_H
#define UM_RF433_CONFIG_H

#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UM_RF433_MAX_SENSORS 32
#define UM_RF433_CONFIG_PATH "/spiffs/rf433.json"

    // Структура runtime данных датчика (только то, что нужно в памяти)
    typedef struct
    {
        uint32_t serial;          // Серийный номер датчика
        long time;                // Время последнего приема
        long last_processed_time; // Время последней обработки
        bool alarm;               // Сигнализация (дублируется из конфига для быстрого доступа)
        bool triggered;           // Флаг срабатывания
        uint8_t state;            // Текущее состояние (каналы)
        uint8_t packet_count;     // Счетчик пакетов для дебаунсинга
    } um_rf433_device_t;

    // Массив всех активных датчиков (runtime)
    extern um_rf433_device_t rf_devices[UM_RF433_MAX_SENSORS];

    /**
     * @brief Загружает конфигурацию RF433 датчиков из файла
     * @return esp_err_t Результат операции
     */
    esp_err_t um_rf433_config_load(void);

    /**
     * @brief Сохраняет текущую конфигурацию датчиков в файл
     * @return esp_err_t Результат операции
     */
    esp_err_t um_rf433_config_save(void);

    /**
     * @brief Создает пустой конфигурационный файл (пустой массив)
     * @return esp_err_t Результат операции
     */
    esp_err_t um_rf433_config_create_empty(void);

    /**
     * @brief Возвращает конфигурацию как строку (нужно освободить через free())
     * @return char* JSON строка или NULL
     */
    char *um_rf433_config_read(void);

    /**
     * @brief Получить runtime данные датчика по серийному номеру
     * @param serial Серийный номер датчика
     * @return um_rf433_device_t* или NULL если не найден
     */
    um_rf433_device_t *um_rf433_config_get_device(uint32_t serial);

    /**
     * @brief Получить индекс датчика в массиве по серийному номеру
     * @param serial Серийный номер датчика
     * @return int Индекс или -1 если не найден
     */
    int um_rf433_config_get_index(uint32_t serial);

    /**
     * @brief Добавить новый датчик в конфигурацию
     * @param serial Серийный номер
     * @param type Тип датчика
     * @param label Название датчика
     * @param alarm Флаг сигнализации
     * @return esp_err_t Результат операции
     */
    esp_err_t um_rf433_config_add_device(uint32_t serial, uint8_t type, const char *label, bool alarm);

    /**
     * @brief Обновить существующий датчик
     * @param serial Серийный номер
     * @param type Тип датчика
     * @param label Название датчика
     * @param alarm Флаг сигнализации
     * @return esp_err_t Результат операции
     */
    esp_err_t um_rf433_config_update_device(uint32_t serial, uint8_t type, const char *label, bool alarm);

    /**
     * @brief Удалить датчик из конфигурации
     * @param serial Серийный номер
     * @return esp_err_t Результат операции
     */
    esp_err_t um_rf433_config_remove_device(uint32_t serial);

    /**
     * @brief Получить количество активных датчиков
     * @return uint8_t Количество датчиков
     */
    uint8_t um_rf433_config_get_count(void);

    /**
     * @brief Очистить runtime массив (сбросить состояния)
     */
    void um_rf433_config_clear_runtime(void);

#ifdef __cplusplus
}
#endif

#endif // UM_RF433_CONFIG_H