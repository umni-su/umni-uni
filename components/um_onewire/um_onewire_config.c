#include "um_onewire_config.h"
#include "um_storage.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "um_onewire_config";

// Хранилище конфигураций
static um_onewire_sensor_config_t sensor_configs[ONEWIRE_MAX_SENSORS];
static uint8_t config_count = 0;
static const char *ow_config_path = "/spiffs/onewire.json";

// Вспомогательная функция для поиска конфигурации
static um_onewire_sensor_config_t *find_config(const char *serial)
{
    for (int i = 0; i < config_count; i++)
    {
        if (strcmp(sensor_configs[i].serial, serial) == 0)
        {
            return &sensor_configs[i];
        }
    }
    return NULL;
}

esp_err_t um_onewire_config_load()
{

    // Проверяем существование файла
    if (!um_storage_file_exists(ow_config_path))
    {
        ESP_LOGW(TAG, "Config file %s not found, creating default", ow_config_path);
        return um_onewire_config_create_default(ow_config_path);
    }

    // Способ 2: Более безопасный
    char *config = um_storage_read_json_string(ow_config_path);
    if (config != NULL)
    {
        // Парсим JSON
        cJSON *root = cJSON_Parse(config);
        if (root == NULL)
        {
            ESP_LOGE(TAG, "Failed to parse JSON config");
            return ESP_ERR_INVALID_RESPONSE;
        }

        // Очищаем старые конфигурации
        config_count = 0;
        memset(sensor_configs, 0, sizeof(sensor_configs));

        // Читаем массив датчиков
        cJSON *sensor_array = cJSON_GetObjectItem(root, "sensors");
        if (cJSON_IsArray(sensor_array))
        {
            cJSON *sensor_item = NULL;
            cJSON_ArrayForEach(sensor_item, sensor_array)
            {
                if (config_count >= ONEWIRE_MAX_SENSORS)
                {
                    ESP_LOGW(TAG, "Too many sensors in config, max is %d", ONEWIRE_MAX_SENSORS);
                    break;
                }

                um_onewire_sensor_config_t *config = &sensor_configs[config_count];

                // Читаем обязательные поля
                cJSON *sn = cJSON_GetObjectItem(sensor_item, "sn");
                cJSON *label = cJSON_GetObjectItem(sensor_item, "label");

                if (cJSON_IsString(sn) && cJSON_IsString(label))
                {
                    strncpy(config->serial, sn->valuestring, sizeof(config->serial) - 1);
                    strncpy(config->label, label->valuestring, sizeof(config->label) - 1);

                    // Читаем опциональные поля
                    cJSON *location = cJSON_GetObjectItem(sensor_item, "location");
                    if (cJSON_IsString(location))
                    {
                        strncpy(config->location, location->valuestring, sizeof(config->location) - 1);
                    }

                    cJSON *active = cJSON_GetObjectItem(sensor_item, "active");
                    config->active = (active && cJSON_IsBool(active)) ? cJSON_IsTrue(active) : true;

                    cJSON *calibration = cJSON_GetObjectItem(sensor_item, "calibration");
                    config->calibration = (calibration && cJSON_IsNumber(calibration)) ? (float)calibration->valuedouble : 0.0f;

                    ESP_LOGI(TAG, "Loaded config for %s: '%s' (active: %s)",
                             config->serial, config->label, config->active ? "yes" : "no");

                    config_count++;
                }
            }
        }
        free(config); // Освобождаем
        cJSON_Delete(root);
        ESP_LOGI(TAG, "Loaded %d sensor configurations", config_count);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t um_onewire_config_save()
{
    // Создаем JSON объект
    cJSON *root = cJSON_CreateObject();
    cJSON *sensors_array = cJSON_CreateArray();

    // Добавляем все конфигурации
    for (int i = 0; i < config_count; i++)
    {
        const um_onewire_sensor_config_t *config = &sensor_configs[i];

        cJSON *sensor = cJSON_CreateObject();
        cJSON_AddStringToObject(sensor, "sn", config->serial);
        cJSON_AddStringToObject(sensor, "label", config->label);

        if (strlen(config->location) > 0)
        {
            cJSON_AddStringToObject(sensor, "location", config->location);
        }

        cJSON_AddBoolToObject(sensor, "active", config->active);

        if (config->calibration != 0.0f)
        {
            cJSON_AddNumberToObject(sensor, "calibration", config->calibration);
        }

        cJSON_AddItemToArray(sensors_array, sensor);
    }

    cJSON_AddItemToObject(root, "sensors", sensors_array);

    // Преобразуем в строку
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL)
    {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    // Сохраняем в файл
    esp_err_t ret = um_storage_write_json(ow_config_path, json_str);

    free(json_str);
    cJSON_Delete(root);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Saved %d sensor configurations to %s", config_count, ow_config_path);
    }

    return ret;
}

void um_onewire_config_apply(void)
{
    const um_onewire_state_t *state = um_onewire_get_state();

    for (int i = 0; i < state->sensor_count; i++)
    {
        const um_onewire_sensor_t *sensor = &state->sensors[i];
        um_onewire_sensor_config_t *config = find_config(sensor->serial);

        if (config)
        {
            // ПРИМЕНЯЕМ КОНФИГ К СОСТОЯНИЮ ONEWIRE
            um_onewire_set_sensor_active(sensor->address, config->active);
            um_onewire_set_sensor_calibration(sensor->address, config->calibration);

            // Можно добавить логирование изменений
            bool current_active;
            float current_calib;

            um_onewire_get_sensor_active(sensor->address, &current_active);
            um_onewire_get_sensor_calibration(sensor->address, &current_calib);

            ESP_LOGI(TAG, "Config applied to %s: active=%s->%s, calib=%.2f->%.2f",
                     sensor->serial,
                     current_active ? "on" : "off",
                     config->active ? "on" : "off",
                     current_calib,
                     config->calibration);
        }
        else
        {
            // Создаём дефолтный конфиг для нового датчика
            // и сразу применяем его
            if (config_count < ONEWIRE_MAX_SENSORS)
            {
                um_onewire_sensor_config_t new_config = {
                    .active = true,
                    .calibration = 0.0f};
                strncpy(new_config.serial, sensor->serial, sizeof(new_config.serial) - 1);
                snprintf(new_config.label, sizeof(new_config.label), "Sensor %s", sensor->serial);

                // Сохраняем в массив конфигов
                sensor_configs[config_count] = new_config;
                config_count++;

                // Применяем к состоянию
                um_onewire_set_sensor_active(sensor->address, true);
                um_onewire_set_sensor_calibration(sensor->address, 0.0f);

                ESP_LOGI(TAG, "Created default config for new sensor %s", sensor->serial);
            }
        }
    }
}

char *um_onewire_config_read(void)
{
    return um_storage_read_json_string(ow_config_path);
}

esp_err_t um_onewire_config_update(const char *serial, const um_onewire_sensor_config_t *config)
{
    if (serial == NULL || config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_onewire_sensor_config_t *existing_config = find_config(serial);
    if (existing_config)
    {
        // Обновляем существующую конфигурацию
        *existing_config = *config;
        strncpy(existing_config->serial, serial, sizeof(existing_config->serial) - 1);
        ESP_LOGI(TAG, "Updated config for %s", serial);
    }
    else
    {
        // Добавляем новую конфигурацию
        if (config_count >= ONEWIRE_MAX_SENSORS)
        {
            return ESP_ERR_NO_MEM;
        }

        sensor_configs[config_count] = *config;
        strncpy(sensor_configs[config_count].serial, serial, sizeof(sensor_configs[config_count].serial) - 1);
        config_count++;
        ESP_LOGI(TAG, "Added new config for %s", serial);
    }

    return ESP_OK;
}

const um_onewire_sensor_config_t *um_onewire_config_get(const char *serial)
{
    return find_config(serial);
}

esp_err_t um_onewire_config_create_default()
{
    // Создаем конфигурацию на основе найденных датчиков
    um_onewire_config_apply();
    return um_onewire_config_save(ow_config_path);
}
