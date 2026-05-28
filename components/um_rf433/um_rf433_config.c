#include "um_rf433_config.h"
#include "um_storage.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "um_rf433_config";

// Массив активных датчиков (runtime)
um_rf433_device_t rf_devices[UM_RF433_MAX_SENSORS] = {0};

// Внутренняя функция для поиска индекса по серийному номеру в runtime массиве
static int find_device_index_by_serial(uint32_t serial)
{
    for (int i = 0; i < UM_RF433_MAX_SENSORS; i++)
    {
        if (rf_devices[i].serial == serial)
        {
            return i;
        }
    }
    return -1;
}

// Внутренняя функция для поиска свободного индекса в runtime массиве
static int find_free_device_index(void)
{
    for (int i = 0; i < UM_RF433_MAX_SENSORS; i++)
    {
        if (rf_devices[i].serial == 0)
        {
            return i;
        }
    }
    return -1;
}

// Внутренняя функция для очистки runtime данных датчика
static void clear_device_data(um_rf433_device_t *dev)
{
    dev->time = 0;
    dev->last_processed_time = 0;
    dev->triggered = false;
    dev->state = 0;
    dev->packet_count = 0;
    // serial и alarm остаются
}

// Инициализация runtime массива из конфига
static void init_runtime_from_config(cJSON *root)
{
    // Очищаем runtime массив
    memset(rf_devices, 0, sizeof(rf_devices));

    cJSON *devices_array = cJSON_GetObjectItem(root, "devices");
    if (!cJSON_IsArray(devices_array))
    {
        ESP_LOGW(TAG, "No devices array in config");
        return;
    }

    int count = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, devices_array)
    {
        if (count >= UM_RF433_MAX_SENSORS)
        {
            ESP_LOGW(TAG, "Too many devices in config, max is %d", UM_RF433_MAX_SENSORS);
            break;
        }

        cJSON *serial = cJSON_GetObjectItem(item, "serial");
        cJSON *alarm = cJSON_GetObjectItem(item, "alarm");

        if (!cJSON_IsNumber(serial))
        {
            ESP_LOGW(TAG, "Invalid device entry in config (missing serial), skipping");
            continue;
        }

        // Заполняем runtime структуру только нужными полями
        rf_devices[count].serial = serial->valueint;
        rf_devices[count].alarm = (alarm && cJSON_IsBool(alarm)) ? cJSON_IsTrue(alarm) : false;

        // Остальные поля обнуляются автоматически (memset выше)
        count++;
    }

    ESP_LOGI(TAG, "Loaded %d RF433 devices from config", count);
}

esp_err_t um_rf433_config_load(void)
{
    // Проверяем существование файла
    if (!um_storage_file_exists(UM_RF433_CONFIG_PATH))
    {
        ESP_LOGW(TAG, "Config file %s not found, creating empty", UM_RF433_CONFIG_PATH);
        return um_rf433_config_create_empty();
    }

    // Читаем файл
    char *json_str = um_storage_read_json_string(UM_RF433_CONFIG_PATH);
    if (json_str == NULL)
    {
        ESP_LOGE(TAG, "Failed to read config file");
        return ESP_FAIL;
    }

    // Парсим JSON
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON config");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Инициализируем runtime массив
    init_runtime_from_config(root);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "RF433 config loaded successfully");
    return ESP_OK;
}

esp_err_t um_rf433_config_save(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *devices_array = cJSON_CreateArray();

    // Читаем текущий конфиг чтобы сохранить type и label
    char *existing_json = um_storage_read_json_string(UM_RF433_CONFIG_PATH);
    cJSON *old_root = NULL;
    cJSON *old_devices = NULL;

    if (existing_json)
    {
        old_root = cJSON_Parse(existing_json);
        if (old_root)
        {
            old_devices = cJSON_GetObjectItem(old_root, "devices");
        }
        free(existing_json);
    }

    // Собираем все активные датчики из runtime массива
    for (int i = 0; i < UM_RF433_MAX_SENSORS; i++)
    {
        if (rf_devices[i].serial == 0)
        {
            continue;
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "serial", rf_devices[i].serial);
        cJSON_AddBoolToObject(item, "alarm", rf_devices[i].alarm);

        // Пытаемся найти type и label из старого конфига
        if (old_devices && cJSON_IsArray(old_devices))
        {
            cJSON *old_item = NULL;
            cJSON_ArrayForEach(old_item, old_devices)
            {
                cJSON *old_serial = cJSON_GetObjectItem(old_item, "serial");
                if (cJSON_IsNumber(old_serial) && old_serial->valueint == rf_devices[i].serial)
                {
                    cJSON *old_type = cJSON_GetObjectItem(old_item, "type");
                    cJSON *old_label = cJSON_GetObjectItem(old_item, "label");

                    if (cJSON_IsNumber(old_type))
                    {
                        cJSON_AddNumberToObject(item, "type", old_type->valueint);
                    }
                    if (cJSON_IsString(old_label))
                    {
                        cJSON_AddStringToObject(item, "label", old_label->valuestring);
                    }
                    break;
                }
            }
        }

        // Если не нашли в старом конфиге, добавляем значения по умолчанию
        if (!cJSON_GetObjectItem(item, "type"))
        {
            cJSON_AddNumberToObject(item, "type", 0);
        }
        if (!cJSON_GetObjectItem(item, "label"))
        {
            cJSON_AddStringToObject(item, "label", "");
        }

        cJSON_AddItemToArray(devices_array, item);
    }

    cJSON_AddItemToObject(root, "devices", devices_array);

    if (old_root)
    {
        cJSON_Delete(old_root);
    }

    // Сохраняем
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = um_storage_write_json(UM_RF433_CONFIG_PATH, json_str);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "RF433 config saved to %s", UM_RF433_CONFIG_PATH);
    }

    return ret;
}

esp_err_t um_rf433_config_create_empty(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "devices", cJSON_CreateArray());

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = um_storage_write_json(UM_RF433_CONFIG_PATH, json_str);
    free(json_str);

    // Очищаем runtime массив
    memset(rf_devices, 0, sizeof(rf_devices));

    return ret;
}

char *um_rf433_config_read(void)
{
    return um_storage_read_json_string(UM_RF433_CONFIG_PATH);
}

char *um_rf433_config_read_with_state(void)
{
    // Читаем исходный JSON конфиг
    char *json_str = um_storage_read_json_string(UM_RF433_CONFIG_PATH);
    if (json_str == NULL)
    {
        ESP_LOGE(TAG, "Failed to read config file");
        return NULL;
    }

    // Парсим JSON
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON config");
        free(json_str);
        return NULL;
    }

    // Получаем массив устройств
    cJSON *devices_array = cJSON_GetObjectItem(root, "devices");
    if (!cJSON_IsArray(devices_array))
    {
        ESP_LOGW(TAG, "No devices array in config");
        cJSON_Delete(root);
        return json_str; // Возвращаем исходный JSON если нет массива
    }

    // Добавляем state для каждого устройства
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, devices_array)
    {
        cJSON *serial_json = cJSON_GetObjectItem(item, "serial");
        if (cJSON_IsNumber(serial_json))
        {
            uint32_t serial = serial_json->valueint;

            static char serial_str[9];

            // Безопасно форматируем uint32_t в HEX-строку
            snprintf(serial_str, sizeof(serial_str), "%06lX", (unsigned long)serial);

            cJSON_AddStringToObject(item, "serial", serial_str);

            // Ищем устройство в runtime массиве
            um_rf433_device_t *dev = um_rf433_config_get_device(serial);
            if (dev != NULL)
            {
                // Добавляем поле state
                cJSON_AddNumberToObject(item, "value", dev->state);
                cJSON_AddNumberToObject(item, "time", dev->time);
                cJSON_AddBoolToObject(item, "triggered", dev->triggered);
            }
            else
            {
                // Устройство не найдено в runtime
                cJSON_AddNumberToObject(item, "value", 0);
                cJSON_AddNumberToObject(item, "time", 0);
                cJSON_AddBoolToObject(item, "triggered", false);
            }
        }
    }

    // Конвертируем обратно в строку
    char *result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(json_str);

    if (result == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON with state");
        return NULL;
    }

    return result;
}

um_rf433_device_t *um_rf433_config_get_device(uint32_t serial)
{
    int index = find_device_index_by_serial(serial);
    return (index >= 0) ? &rf_devices[index] : NULL;
}

int um_rf433_config_get_index(uint32_t serial)
{
    return find_device_index_by_serial(serial);
}

esp_err_t um_rf433_config_add_device(uint32_t serial, uint8_t type, const char *label, bool alarm)
{
    int index = find_device_index_by_serial(serial);

    // Читаем текущий конфиг
    char *existing_json = um_storage_read_json_string(UM_RF433_CONFIG_PATH);
    cJSON *root = NULL;
    cJSON *devices_array = NULL;

    if (existing_json)
    {
        root = cJSON_Parse(existing_json);
        if (root)
        {
            devices_array = cJSON_GetObjectItem(root, "devices");
        }
        free(existing_json);
    }

    if (!root)
    {
        root = cJSON_CreateObject();
        devices_array = cJSON_CreateArray();
        cJSON_AddItemToObject(root, "devices", devices_array);
    }

    // Ищем или создаем устройство в JSON
    cJSON *device_item = NULL;
    if (cJSON_IsArray(devices_array))
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, devices_array)
        {
            cJSON *serial_json = cJSON_GetObjectItem(item, "serial");
            if (cJSON_IsNumber(serial_json) && serial_json->valueint == serial)
            {
                device_item = item;
                break;
            }
        }
    }

    if (device_item)
    {
        // Обновляем существующий
        cJSON_ReplaceItemInObject(device_item, "type", cJSON_CreateNumber(type));
        cJSON_ReplaceItemInObject(device_item, "label", cJSON_CreateString(label ? label : ""));
        cJSON_ReplaceItemInObject(device_item, "alarm", cJSON_CreateBool(alarm));
        ESP_LOGI(TAG, "Update device %06lX in config", serial);
    }
    else
    {
        // Добавляем новый
        cJSON *new_item = cJSON_CreateObject();
        cJSON_AddNumberToObject(new_item, "serial", serial);
        cJSON_AddNumberToObject(new_item, "type", type);
        cJSON_AddStringToObject(new_item, "label", label ? label : "");
        cJSON_AddBoolToObject(new_item, "alarm", alarm);
        cJSON_AddItemToArray(devices_array, new_item);
        ESP_LOGI(TAG, "Add new device %06lX to config", serial);
    }

    // Сохраняем JSON
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = um_storage_write_json(UM_RF433_CONFIG_PATH, json_str);
    free(json_str);

    if (ret != ESP_OK)
    {
        return ret;
    }

    // Обновляем runtime массив (только serial и alarm)
    if (index >= 0)
    {
        // Обновляем alarm в существующем
        rf_devices[index].alarm = alarm;
    }
    else
    {
        // Добавляем новый в runtime
        int free_index = find_free_device_index();
        if (free_index >= 0)
        {
            rf_devices[free_index].serial = serial;
            rf_devices[free_index].alarm = alarm;
            clear_device_data(&rf_devices[free_index]);
        }
        else
        {
            ESP_LOGW(TAG, "No free runtime slot for device %06lX", serial);
        }
    }

    return ESP_OK;
}

esp_err_t um_rf433_config_update_device(uint32_t serial, uint8_t type, const char *label, bool alarm)
{
    // Просто вызываем add_device, который сделает upsert
    return um_rf433_config_add_device(serial, type, label, alarm);
}

esp_err_t um_rf433_config_remove_device(uint32_t serial)
{
    int index = find_device_index_by_serial(serial);

    // Удаляем из runtime
    if (index >= 0)
    {
        memset(&rf_devices[index], 0, sizeof(um_rf433_device_t));
    }

    // Удаляем из JSON конфига
    char *existing_json = um_storage_read_json_string(UM_RF433_CONFIG_PATH);
    if (!existing_json)
    {
        return ESP_ERR_NOT_FOUND;
    }

    cJSON *root = cJSON_Parse(existing_json);
    free(existing_json);

    if (!root)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *devices_array = cJSON_GetObjectItem(root, "devices");
    if (cJSON_IsArray(devices_array))
    {
        int array_size = cJSON_GetArraySize(devices_array);
        for (int i = 0; i < array_size; i++)
        {
            cJSON *item = cJSON_GetArrayItem(devices_array, i);
            cJSON *serial_json = cJSON_GetObjectItem(item, "serial");
            if (cJSON_IsNumber(serial_json) && serial_json->valueint == serial)
            {
                cJSON_DeleteItemFromArray(devices_array, i);
                break;
            }
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = um_storage_write_json(UM_RF433_CONFIG_PATH, json_str);
    free(json_str);

    ESP_LOGI(TAG, "Removed device %06lX", serial);

    return ret;
}

uint8_t um_rf433_config_get_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < UM_RF433_MAX_SENSORS; i++)
    {
        if (rf_devices[i].serial != 0)
        {
            count++;
        }
    }
    return count;
}

void um_rf433_config_clear_runtime(void)
{
    for (int i = 0; i < UM_RF433_MAX_SENSORS; i++)
    {
        if (rf_devices[i].serial != 0)
        {
            clear_device_data(&rf_devices[i]);
        }
    }
    ESP_LOGI(TAG, "Runtime data cleared for all devices");
}