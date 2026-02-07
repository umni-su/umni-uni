#include "um_onewire.h"
#include <string.h>
#include <inttypes.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#if defined(CONFIG_UM_FEATURE_ONEWIRE)

static const char *TAG = "onewire";

// Глобальное состояние шины
static um_onewire_state_t onewire_state = {0};

/**
 * @brief Вспомогательная функция для получения типа датчика по family ID
 */
static um_onewire_sensor_type_t get_sensor_type(uint8_t family_id)
{
    switch (family_id)
    {
    case DS18X20_FAMILY_DS18S20:
        return UM_ONEWIRE_TYPE_DS18S20;
    case DS18X20_FAMILY_DS1822:
        return UM_ONEWIRE_TYPE_DS1822;
    case DS18X20_FAMILY_DS18B20:
        return UM_ONEWIRE_TYPE_DS18B20;
    case DS18X20_FAMILY_MAX31850:
        return UM_ONEWIRE_TYPE_MAX31850;
    default:
        return UM_ONEWIRE_TYPE_UNKNOWN;
    }
}

esp_err_t um_onewire_init(void)
{
    ESP_LOGI(TAG, "Initializing 1-Wire bus on GPIO %d", ONE_WIRE_PIN);

    // Инициализируем состояние
    memset(&onewire_state, 0, sizeof(um_onewire_state_t));

    // Явно инициализируем все поля структуры
    onewire_state.sensor_count = 0;
    onewire_state.initialized = true;

    // Инициализируем все датчики в массиве (даже неиспользуемые)
    for (int i = 0; i < ONEWIRE_MAX_SENSORS; i++)
    {
        onewire_state.sensors[i].address = 0;
        onewire_state.sensors[i].type = UM_ONEWIRE_TYPE_UNKNOWN;
        onewire_state.sensors[i].temperature = 0.0f;
        onewire_state.sensors[i].active = true;      // По умолчанию все активны
        onewire_state.sensors[i].calibration = 0.0f; // Без калибровки по умолчанию
        memset(onewire_state.sensors[i].serial, 0, sizeof(onewire_state.sensors[i].serial));
    }

    // Настраиваем подтягивающий резистор
    gpio_set_pull_mode(ONE_WIRE_PIN, GPIO_PULLUP_ONLY);

    // Выполняем первое сканирование
    uint8_t count = um_onewire_scan();
    ESP_LOGI(TAG, "Initial scan found %d sensors", count);

    return ESP_OK;
}

void um_onewire_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing 1-Wire bus");
    memset(&onewire_state, 0, sizeof(um_onewire_state_t));
    onewire_state.initialized = false;
}

uint8_t um_onewire_scan(void)
{
    if (!onewire_state.initialized)
    {
        ESP_LOGW(TAG, "1-Wire bus not initialized");
        return 0;
    }

    onewire_search_t search;
    onewire_addr_t addr;
    uint8_t found = 0;

    // Начинаем поиск
    onewire_search_start(&search);

    // Очищаем предыдущие данные
    memset(onewire_state.sensors, 0, sizeof(onewire_state.sensors));
    onewire_state.sensor_count = 0;

    // Ищем все устройства на шине
    while ((addr = onewire_search_next(&search, ONE_WIRE_PIN)) != ONEWIRE_NONE)
    {
        if (found < ONEWIRE_MAX_SENSORS)
        {
            uint8_t family_id = (uint8_t)addr;
            um_onewire_sensor_type_t type = get_sensor_type(family_id);

            // Сохраняем только поддерживаемые датчики температуры
            if (type != UM_ONEWIRE_TYPE_UNKNOWN)
            {
                onewire_state.sensors[found].address = addr;
                onewire_state.sensors[found].type = type;
                onewire_state.sensors[found].active = true;
                onewire_state.sensors[found].temperature = 0.0;

                // Преобразуем адрес в строку
                um_onewire_address_to_string(addr, onewire_state.sensors[found].serial);

                ESP_LOGI(TAG, "Found sensor: %s (type: %s)",
                         onewire_state.sensors[found].serial,
                         um_onewire_sensor_type_to_string(type));

                found++;
            }
            else
            {
                ESP_LOGW(TAG, "Found unsupported device with family ID: 0x%02X", family_id);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Maximum number of sensors reached (%d)", ONEWIRE_MAX_SENSORS);
            break;
        }
    }

    onewire_state.sensor_count = found;

    if (found == 0)
    {
        ESP_LOGW(TAG, "No temperature sensors found on 1-Wire bus");
    }

    return found;
}

const um_onewire_state_t *um_onewire_get_state(void)
{
    return &onewire_state;
}

uint8_t um_onewire_get_sensor_count(void)
{
    return onewire_state.sensor_count;
}

const um_onewire_sensor_t *um_onewire_get_sensor(uint8_t index)
{
    if (index < onewire_state.sensor_count)
    {
        return &onewire_state.sensors[index];
    }
    return NULL;
}

esp_err_t um_onewire_read_all_temperatures(void)
{
    if (!onewire_state.initialized || onewire_state.sensor_count == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Создаем массив адресов для чтения
    onewire_addr_t addresses[ONEWIRE_MAX_SENSORS];
    float temperatures[ONEWIRE_MAX_SENSORS];

    for (uint8_t i = 0; i < onewire_state.sensor_count; i++)
    {
        addresses[i] = onewire_state.sensors[i].address;
    }

    // Читаем температуру со всех датчиков
    esp_err_t res = ds18x20_measure_and_read_multi(ONE_WIRE_PIN, addresses,
                                                   onewire_state.sensor_count, temperatures);

    if (res == ESP_OK)
    {
        for (uint8_t i = 0; i < onewire_state.sensor_count; i++)
        {
            onewire_state.sensors[i].temperature = temperatures[i];
            ESP_LOGI(TAG, "Sensor %s: %.2f°C",
                     onewire_state.sensors[i].serial,
                     temperatures[i]);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read temperatures: %s", esp_err_to_name(res));
    }

    return res;
}

esp_err_t um_onewire_read_temperature(uint64_t address, float *temperature)
{
    if (!onewire_state.initialized || temperature == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Ищем датчик с таким адресом
    for (uint8_t i = 0; i < onewire_state.sensor_count; i++)
    {
        if (onewire_state.sensors[i].address == address)
        {
            esp_err_t res = ds18x20_measure_and_read(ONE_WIRE_PIN, address, temperature);
            if (res == ESP_OK)
            {
                onewire_state.sensors[i].temperature = *temperature;
            }
            return res;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t um_onewire_set_sensor_active(uint64_t address, bool active)
{
    if (!onewire_state.initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < onewire_state.sensor_count; i++)
    {
        if (onewire_state.sensors[i].address == address)
        {
            onewire_state.sensors[i].active = active;
            ESP_LOGI(TAG, "Sensor %016" PRIX64 " active: %s",
                     address, active ? "true" : "false");
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t um_onewire_set_sensor_calibration(uint64_t address, float calibration)
{
    if (!onewire_state.initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Проверяем разумные пределы калибровки
    if (calibration < -10.0f || calibration > 10.0f)
    {
        ESP_LOGW(TAG, "Calibration value %.2f out of reasonable range", calibration);
        // Но не запрещаем, возможно спецслучай
    }

    for (int i = 0; i < onewire_state.sensor_count; i++)
    {
        if (onewire_state.sensors[i].address == address)
        {
            onewire_state.sensors[i].calibration = calibration;
            ESP_LOGI(TAG, "Sensor %016" PRIX64 " calibration: %+.2f°C",
                     address, calibration);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t um_onewire_get_sensor_calibration(uint64_t address, float *calibration)
{
    if (!onewire_state.initialized || calibration == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < onewire_state.sensor_count; i++)
    {
        if (onewire_state.sensors[i].address == address)
        {
            *calibration = onewire_state.sensors[i].calibration;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t um_onewire_get_sensor_active(uint64_t address, bool *active)
{
    if (!onewire_state.initialized || active == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < onewire_state.sensor_count; i++)
    {
        if (onewire_state.sensors[i].address == address)
        {
            *active = onewire_state.sensors[i].active;
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

// Также обновляем функцию для получения калиброванной температуры:
float um_onewire_get_calibrated_temperature(const um_onewire_sensor_t *sensor)
{
    if (!sensor || !sensor->active)
    {
        return 0.0f;
    }
    return sensor->temperature + sensor->calibration;
}

void um_onewire_address_to_string(uint64_t address, char *buffer)
{
    if (buffer != NULL)
    {
        snprintf(buffer, 17, "%016" PRIX64, address);
        buffer[16] = '\0'; // Гарантируем нуль-терминацию
    }
}

esp_err_t um_onewire_string_to_address(const char *str, uint64_t *address)
{
    if (str == NULL || address == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char *endptr;
    *address = strtoull(str, &endptr, 16);

    if (endptr == str || *endptr != '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

const char *um_onewire_sensor_type_to_string(um_onewire_sensor_type_t type)
{
    switch (type)
    {
    case UM_ONEWIRE_TYPE_DS18S20:
        return "DS18S20";
    case UM_ONEWIRE_TYPE_DS1822:
        return "DS1822";
    case UM_ONEWIRE_TYPE_DS18B20:
        return "DS18B20";
    case UM_ONEWIRE_TYPE_MAX31850:
        return "MAX31850";
    default:
        return "Unknown";
    }
}

#endif