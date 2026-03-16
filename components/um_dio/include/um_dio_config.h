#ifndef UM_DIO_CONFIG_H
#define UM_DIO_CONFIG_H

#include "cJSON.h"
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Структура конфигурации одного DIO канала
    typedef struct
    {
        uint8_t config_index; // Индекс в конфигурации (1-6 для входов, 1-8 для выходов)
        uint8_t port_index;   // Физический порт на PCF8574 (1-8)
        char label[32];       // Человеко-читаемое имя
        bool active;          // Активен ли канал
        int default_state;    // Состояние по умолчанию для выходов (0/1)
    } um_dio_config_item_t;

    // Конфигурация всех DIO каналов
    typedef struct
    {
        um_dio_config_item_t inputs[6];  // Максимум 6 входов (индексы 1-6)
        um_dio_config_item_t outputs[8]; // Максимум 8 выходов (индексы 1-8)
        uint8_t input_count;             // Реальное кол-во входов
        uint8_t output_count;            // Реальное кол-во выходов
    } um_dio_config_t;

    /**
     * @brief Загружает конфигурацию DIO из файла
     * @return esp_err_t Результат операции
     */
    esp_err_t um_dio_config_load(void);

    /**
     * @brief Сохраняет текущую конфигурацию DIO в файл
     * @return esp_err_t Результат операции
     */
    esp_err_t um_dio_config_save(void);

    /**
     * @brief Создает конфигурационный файл на основе возможностей устройства и маппинга
     * @return esp_err_t Результат операции
     */
    esp_err_t um_dio_config_create_default(void);

    /**
     * @brief Возвращает конфигурацию как строку (нужно освободить через free())
     * @return char* JSON строка или NULL
     */
    char *um_dio_config_read(void);

    /**
     * @brief Получить конфигурацию входа по его конфигурационному индексу (1-6)
     * @param config_index Конфигурационный индекс входа (1-6)
     * @return const um_dio_config_item_t* или NULL
     */
    const um_dio_config_item_t *um_dio_config_get_input(uint8_t config_index);

    /**
     * @brief Получить конфигурацию выхода по его конфигурационному индексу (1-8)
     * @param config_index Конфигурационный индекс выхода (1-8)
     * @return const um_dio_config_item_t* или NULL
     */
    const um_dio_config_item_t *um_dio_config_get_output(uint8_t config_index);

    /**
     * @brief Обновить конфигурацию входа
     * @param config_index Конфигурационный индекс входа (1-6)
     * @param label Новое название (можно NULL)
     * @param active Новое состояние активности
     * @return esp_err_t Результат операции
     */
    esp_err_t um_dio_config_update_input(uint8_t config_index, const char *label, bool active);

    /**
     * @brief Обновить конфигурацию выхода
     * @param config_index Конфигурационный индекс выхода (1-8)
     * @param label Новое название (можно NULL)
     * @param active Новое состояние активности
     * @param default_state Состояние по умолчанию
     * @return esp_err_t Результат операции
     */
    esp_err_t um_dio_config_update_output(uint8_t config_index, const char *label, bool active, int default_state);

    /**
     * @brief Получить портовый индекс по конфигурационному индексу входа
     * @param config_index Конфигурационный индекс входа (1-6)
     * @return uint8_t Порт (1-8) или 0 если не найден
     */
    uint8_t um_dio_config_get_input_port(uint8_t config_index);

    /**
     * @brief Получить портовый индекс по конфигурационному индексу выхода
     * @param config_index Конфигурационный индекс выхода (1-8)
     * @return uint8_t Порт (1-8) или 0 если не найден
     */
    uint8_t um_dio_config_get_output_port(uint8_t config_index);

    /**
     * @brief Возвращает конфигурацию входов как JSON строку
     * @return char* JSON строка или NULL (нужно освободить через free())
     */
    char *um_dio_config_get_inputs_json(void);

    /**
     * @brief Возвращает конфигурацию выходов как JSON строку
     * @return char* JSON строка или NULL (нужно освободить через free())
     */
    char *um_dio_config_get_outputs_json(void);

#ifdef __cplusplus
}
#endif

#endif // UM_DIO_CONFIG_H