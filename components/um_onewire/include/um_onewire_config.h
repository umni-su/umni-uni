#ifndef UM_ONEWIRE_CONFIG_H
#define UM_ONEWIRE_CONFIG_H

#include "um_onewire.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Структура конфигурации одного датчика
    typedef struct
    {
        char serial[17];   // Серийный номер
        char label[32];    // Человеко-читаемое имя
        char location[32]; // Местоположение
        bool active;       // Активен ли датчик
        float calibration; // Калибровочное смещение
    } um_onewire_sensor_config_t;

    /**
     * @brief Загружает конфигурацию датчиков из файла
     *
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_config_load();

    /**
     * @brief Сохраняет текущую конфигурацию датчиков в файл
     *
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_config_save();

    /**
     * @brief Применяет загруженную конфигурацию к найденным датчикам
     */
    void um_onewire_config_apply(void);

    /**
     * @brief Возвращает конфигурацию как строку
     *
     * @return char or NULL
     */
    char *um_onewire_config_read(void);

    /**
     * @brief Обновляет конфигурацию конкретного датчика
     *
     * @param serial Серийный номер датчика
     * @param config Новая конфигурация
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_config_update(const char *serial, const um_onewire_sensor_config_t *config);

    /**
     * @brief Возвращает конфигурацию датчика по серийному номеру
     *
     * @param serial Серийный номер датчика
     * @return const um_onewire_sensor_config_t* Указатель на конфигурацию или NULL
     */
    const um_onewire_sensor_config_t *um_onewire_config_get(const char *serial);

    /**
     * @brief Создает конфигурационный файл на основе найденных датчиков
     *
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_config_create_default();

#ifdef __cplusplus
}
#endif

#endif // UM_ONEWIRE_CONFIG_H