#ifndef UM_STORE_H
#define UM_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UM_STORE_MAX_SLOTS 20    // Максимум записей
#define UM_STORE_MAX_NAME_LEN 32 // Максимальная длина имени хранилища
#define UM_STORE_MAX_STORES 10   // Максимум разных хранилищ

    /**
     * @brief Структура одной записи
     */
    typedef struct
    {
        uint64_t timestamp_ms; // Unix timestamp в миллисекундах
        float value;           // Значение
    } um_store_entry_t;

    /**
     * @brief Структура хранилища
     */
    typedef struct
    {
        char name[UM_STORE_MAX_NAME_LEN];             // Имя хранилища ("ntc", "adc")
        um_store_entry_t entries[UM_STORE_MAX_SLOTS]; // Кольцевой буфер
        uint8_t head;                                 // Индекс следующей записи
        uint8_t count;                                // Количество записей (0-20)
        bool initialized;                             // Инициализировано ли
    } um_store_t;

    /**
     * @brief Создать или открыть хранилище
     * @param name Имя хранилища
     * @return Указатель на хранилище или NULL
     */
    um_store_t *um_store_create(const char *name);

    /**
     * @brief Получить хранилище
     * @param name Имя хранилища
     * @return Указатель на хранилище или NULL
     */
    um_store_t *um_store_find_store(const char *name);

    /**
     * @brief Добавить значение в хранилище
     * @param store Указатель на хранилище
     * @param value Значение
     * @return ESP_OK при успехе
     */
    esp_err_t um_store_add_value(um_store_t *store, float value);

    /**
     * @brief Добавить значение с произвольным timestamp
     * @param store Указатель на хранилище
     * @param value Значение
     * @param timestamp_ms Timestamp (0 = текущее время)
     * @return ESP_OK при успехе
     */
    esp_err_t um_store_add_value_with_time(um_store_t *store, float value, uint64_t timestamp_ms);

    /**
     * @brief Получить все записи из хранилища
     * @param store Указатель на хранилище
     * @param entries Указатель на массив для заполнения
     * @param max_count Максимальное количество записей
     * @return Реальное количество записей
     */
    uint8_t um_store_get_all(um_store_t *store, um_store_entry_t *entries, uint8_t max_count);

    /**
     * @brief Получить последние N записей
     * @param store Указатель на хранилище
     * @param entries Указатель на массив
     * @param count Количество записей (1-20)
     * @return Реальное количество полученных записей
     */
    uint8_t um_store_get_last(um_store_t *store, um_store_entry_t *entries, uint8_t count);

    /**
     * @brief Сохранить хранилище на диск (опционально)
     * @param store Указатель на хранилище
     * @return ESP_OK при успехе
     */
    esp_err_t um_store_save(um_store_t *store);

    /**
     * @brief Загрузить хранилище с диска
     * @param store Указатель на хранилище
     * @return ESP_OK при успехе
     */
    esp_err_t um_store_load(um_store_t *store);

    /**
     * @brief Очистить хранилище
     * @param store Указатель на хранилище
     */
    void um_store_clear(um_store_t *store);

    /**
     * @brief Получить данные в формате JSON для ECharts
     * @param store Указатель на хранилище
     * @return JSON строка (нужно освободить free())
     */
    char *um_store_to_json(um_store_t *store);

    /**
     * @brief Статистика хранилища
     * @param store Указатель на хранилище
     * @param min Минимальное значение
     * @param max Максимальное значение
     * @param avg Среднее значение
     */
    void um_store_stats(um_store_t *store, float *min, float *max, float *avg);

#ifdef __cplusplus
}
#endif

#endif // UM_STORE_H