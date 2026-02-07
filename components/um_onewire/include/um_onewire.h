#ifndef UM_ONEWIRE_H
#define UM_ONEWIRE_H

#include <stdint.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include "onewire.h"
#include "ds18x20.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(CONFIG_UM_FEATURE_ONEWIRE)

// Максимальное количество датчиков
#ifndef ONEWIRE_MAX_SENSORS
// TODO move to sdkconfig
#define ONEWIRE_MAX_SENSORS 16
#endif

// Пин для шины 1-Wire
#ifndef ONE_WIRE_PIN
#define ONE_WIRE_PIN CONFIG_UM_CFG_ONEWIRE_GPIO
#endif

    // Типы датчиков
    typedef enum
    {
        UM_ONEWIRE_TYPE_UNKNOWN = 0,
        UM_ONEWIRE_TYPE_DS18S20 = DS18X20_FAMILY_DS18S20,
        UM_ONEWIRE_TYPE_DS1822 = DS18X20_FAMILY_DS1822,
        UM_ONEWIRE_TYPE_DS18B20 = DS18X20_FAMILY_DS18B20,
        UM_ONEWIRE_TYPE_MAX31850 = DS18X20_FAMILY_MAX31850
    } um_onewire_sensor_type_t;

    // Структура датчика
    typedef struct
    {
        uint64_t address;              // Адрес датчика
        um_onewire_sensor_type_t type; // Тип датчика
        float temperature;             // Последнее измеренное значение температуры
        bool active;                   // Активен ли датчик
        float calibration;             // Калибровка
        char serial[17];               // Серийный номер в виде строки
    } um_onewire_sensor_t;

    // Структура состояния шины
    typedef struct
    {
        um_onewire_sensor_t sensors[ONEWIRE_MAX_SENSORS];
        uint8_t sensor_count;
        bool initialized;
    } um_onewire_state_t;

    /**
     * @brief Инициализирует шину 1-Wire
     *
     * @return esp_err_t Результат инициализации
     */
    esp_err_t um_onewire_init(void);

    /**
     * @brief Деинициализирует шину 1-Wire
     */
    void um_onewire_deinit(void);

    /**
     * @brief Сканирует шину на наличие датчиков
     *
     * @return uint8_t Количество найденных датчиков
     */
    uint8_t um_onewire_scan(void);

    /**
     * @brief Возвращает состояние шины
     *
     * @return const um_onewire_state_t* Указатель на состояние шины
     */
    const um_onewire_state_t *um_onewire_get_state(void);

    /**
     * @brief Устанавливает активность датчика
     *
     * @param address Адрес датчика
     * @param active Активен ли датчик
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_set_sensor_active(uint64_t address, bool active);

    /**
     * @brief Устанавливает калибровочное смещение для датчика
     *
     * @param address Адрес датчика
     * @param calibration Калибровочное смещение (°C)
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_set_sensor_calibration(uint64_t address, float calibration);

    /**
     * @brief Возвращает калибровочное смещение датчика
     *
     * @param address Адрес датчика
     * @param calibration Указатель для сохранения калибровки
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_get_sensor_calibration(uint64_t address, float *calibration);

    /**
     * @brief Возвращает активность датчика
     *
     * @param address Адрес датчика
     * @param active Указатель для сохранения активности
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_get_sensor_active(uint64_t address, bool *active);

    /**
     * @brief Возвращает количество найденных датчиков
     *
     * @return uint8_t Количество датчиков
     */
    uint8_t um_onewire_get_sensor_count(void);

    /**
     * @brief Возвращает информацию о конкретном датчике
     *
     * @param index Индекс датчика (0..sensor_count-1)
     * @return const um_onewire_sensor_t* Указатель на датчик или NULL
     */
    const um_onewire_sensor_t *um_onewire_get_sensor(uint8_t index);

    /**
     * @brief Читает температуру со всех датчиков
     *
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_read_all_temperatures(void);

    /**
     * @brief Читает температуру с конкретного датчика
     *
     * @param address Адрес датчика
     * @param temperature Указатель для сохранения температуры
     * @return esp_err_t Результат операции
     */
    esp_err_t um_onewire_read_temperature(uint64_t address, float *temperature);

    /**
     * @brief Преобразует адрес датчика в строку
     *
     * @param address Адрес датчика
     * @param buffer Буфер для строки (минимум 17 байт)
     */
    void um_onewire_address_to_string(uint64_t address, char *buffer);

    /**
     * @brief Преобразует строку в адрес датчика
     *
     * @param str Строка с адресом
     * @param address Указатель для сохранения адреса
     * @return esp_err_t Результат преобразования
     */
    esp_err_t um_onewire_string_to_address(const char *str, uint64_t *address);

    /**
     * @brief Возвращает строковое представление типа датчика
     *
     * @param type Тип датчика
     * @return const char* Название типа датчика
     */
    const char *um_onewire_sensor_type_to_string(um_onewire_sensor_type_t type);

    /**
     * @brief Возвращает калиброванное значение температуры
     *
     * @param sensor Указатель на датчик
     * @return float Калиброванная температура
     */
    float um_onewire_get_calibrated_temperature(const um_onewire_sensor_t *sensor);

#ifdef __cplusplus
}
#endif

#endif

#endif // UM_ONEWIRE_H