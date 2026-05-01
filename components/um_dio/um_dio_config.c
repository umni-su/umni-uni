#include "um_dio.h"
#include "um_dio_config.h"
#include "um_storage.h"
#include "um_capabilities.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "um_dio_config";
static const char *DIO_CONFIG_PATH = "/spiffs/dio.json";

// Хранилище конфигурации
static um_dio_config_t dio_config = {
    .input_count = 0,
    .output_count = 0};

// Внешние массивы маппинга из um_dio.c
extern const uint8_t input_index_map[];  // config_index -> port_index
extern const uint8_t output_index_map[]; // config_index -> port_index

static void update_counts_from_capabilities(void)
{
    dio_config.input_count = 0;
    dio_config.output_count = 0;

    // Для входов - СОХРАНЯЕМ ПОРЯДОК config_index
    // Не перебираем все возможные индексы, а используем маппинг
    for (int config_idx = 1; config_idx <= 6; config_idx++)
    {
        um_capability_t cap = UM_CAP_INP1 + (config_idx - 1);
        if (um_capabilities_has(cap))
        {
            int array_pos = dio_config.input_count;
            dio_config.inputs[array_pos].config_index = config_idx;
            // port_index берем из глобального маппинга
            dio_config.inputs[array_pos].port_index = input_index_map[config_idx - 1];
            dio_config.input_count++;
        }
    }

    // Аналогично для выходов
    for (int config_idx = 1; config_idx <= 8; config_idx++)
    {
        um_capability_t cap = UM_CAP_OUT1 + (config_idx - 1);
        if (um_capabilities_has(cap))
        {
            int array_pos = dio_config.output_count;
            dio_config.outputs[array_pos].config_index = config_idx;
            dio_config.outputs[array_pos].port_index = output_index_map[config_idx - 1];
            dio_config.output_count++;
        }
    }
}
// Поиск входа по конфигурационному индексу
static um_dio_config_item_t *find_input_by_config_index(uint8_t config_index)
{
    for (int i = 0; i < dio_config.input_count; i++)
    {
        if (dio_config.inputs[i].config_index == config_index)
        {
            return &dio_config.inputs[i];
        }
    }
    return NULL;
}

// Поиск выхода по конфигурационному индексу
static um_dio_config_item_t *find_output_by_config_index(uint8_t config_index)
{
    for (int i = 0; i < dio_config.output_count; i++)
    {
        if (dio_config.outputs[i].config_index == config_index)
        {
            return &dio_config.outputs[i];
        }
    }
    return NULL;
}

esp_err_t um_dio_config_load(void)
{
    // Обновляем counts из capabilities
    update_counts_from_capabilities();

    // Проверяем существование файла
    if (!um_storage_file_exists(DIO_CONFIG_PATH))
    {
        ESP_LOGW(TAG, "Config file %s not found, creating default", DIO_CONFIG_PATH);
        return um_dio_config_create_default();
    }

    // Читаем файл
    char *json_str = um_storage_read_json_string(DIO_CONFIG_PATH);
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

    // Читаем входы
    cJSON *inputs_array = cJSON_GetObjectItem(root, "inputs");
    if (cJSON_IsArray(inputs_array))
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, inputs_array)
        {
            cJSON *config_index_json = cJSON_GetObjectItem(item, "config_index");
            if (!cJSON_IsNumber(config_index_json))
                continue;

            uint8_t config_idx = config_index_json->valueint;
            um_dio_config_item_t *input = find_input_by_config_index(config_idx);

            if (input)
            {
                cJSON *label = cJSON_GetObjectItem(item, "label");
                if (cJSON_IsString(label))
                {
                    strncpy(input->label, label->valuestring, sizeof(input->label) - 1);
                }

                cJSON *active = cJSON_GetObjectItem(item, "active");
                input->active = (active && cJSON_IsBool(active)) ? cJSON_IsTrue(active) : true;
            }
        }
    }

    // Читаем выходы
    cJSON *outputs_array = cJSON_GetObjectItem(root, "outputs");
    if (cJSON_IsArray(outputs_array))
    {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, outputs_array)
        {
            cJSON *config_index_json = cJSON_GetObjectItem(item, "config_index");
            if (!cJSON_IsNumber(config_index_json))
                continue;

            uint8_t config_idx = config_index_json->valueint;
            um_dio_config_item_t *output = find_output_by_config_index(config_idx);

            if (output)
            {
                cJSON *label = cJSON_GetObjectItem(item, "label");
                if (cJSON_IsString(label))
                {
                    strncpy(output->label, label->valuestring, sizeof(output->label) - 1);
                }

                cJSON *active = cJSON_GetObjectItem(item, "active");
                output->active = (active && cJSON_IsBool(active)) ? cJSON_IsTrue(active) : true;

                cJSON *default_state = cJSON_GetObjectItem(item, "default_state");
                output->default_state = (default_state && cJSON_IsNumber(default_state)) ? default_state->valueint : 0;
            }
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "DIO config loaded successfully");
    return ESP_OK;
}

esp_err_t um_dio_config_save(void)
{
    cJSON *root = cJSON_CreateObject();

    // Массив входов
    cJSON *inputs_array = cJSON_CreateArray();
    for (int i = 0; i < dio_config.input_count; i++)
    {
        if (strlen(dio_config.inputs[i].label) > 0)
        {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "config_index", dio_config.inputs[i].config_index);
            cJSON_AddNumberToObject(item, "port_index", dio_config.inputs[i].port_index);
            cJSON_AddStringToObject(item, "label", dio_config.inputs[i].label);
            cJSON_AddBoolToObject(item, "active", dio_config.inputs[i].active);
            cJSON_AddItemToArray(inputs_array, item);
        }
    }
    cJSON_AddItemToObject(root, "inputs", inputs_array);

    // Массив выходов
    cJSON *outputs_array = cJSON_CreateArray();
    for (int i = 0; i < dio_config.output_count; i++)
    {
        if (strlen(dio_config.outputs[i].label) > 0)
        {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "config_index", dio_config.outputs[i].config_index);
            cJSON_AddNumberToObject(item, "port_index", dio_config.outputs[i].port_index);
            cJSON_AddStringToObject(item, "label", dio_config.outputs[i].label);
            cJSON_AddBoolToObject(item, "active", dio_config.outputs[i].active);
            cJSON_AddNumberToObject(item, "default_state", dio_config.outputs[i].default_state);
            cJSON_AddItemToArray(outputs_array, item);
        }
    }
    cJSON_AddItemToObject(root, "outputs", outputs_array);

    // Сохраняем
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = um_storage_write_json(DIO_CONFIG_PATH, json_str);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "DIO config saved to %s", DIO_CONFIG_PATH);
    }

    return ret;
}

esp_err_t um_dio_config_create_default(void)
{
    // Обновляем counts и маппинг
    update_counts_from_capabilities();

    // Создаем дефолтные названия для входов
    for (int i = 0; i < dio_config.input_count; i++)
    {
        snprintf(dio_config.inputs[i].label, sizeof(dio_config.inputs[i].label),
                 "Input %d (port %d)",
                 dio_config.inputs[i].config_index,
                 dio_config.inputs[i].port_index);
        dio_config.inputs[i].active = true;
    }

    // Создаем дефолтные названия для выходов
    for (int i = 0; i < dio_config.output_count; i++)
    {
        snprintf(dio_config.outputs[i].label, sizeof(dio_config.outputs[i].label),
                 "Output %d (port %d)",
                 dio_config.outputs[i].config_index,
                 dio_config.outputs[i].port_index);
        dio_config.outputs[i].active = true;
        dio_config.outputs[i].default_state = 0;
    }

    return um_dio_config_save();
}

char *um_dio_config_read(void)
{
    return um_storage_read_json_string(DIO_CONFIG_PATH);
}

const um_dio_config_item_t *um_dio_config_get_input(uint8_t config_index)
{
    if (config_index < 1 || config_index > 6)
    {
        return NULL;
    }
    return find_input_by_config_index(config_index);
}

const um_dio_config_item_t *um_dio_config_get_output(uint8_t config_index)
{
    if (config_index < 1 || config_index > 8)
    {
        return NULL;
    }
    return find_output_by_config_index(config_index);
}

esp_err_t um_dio_config_update_input(uint8_t config_index, const char *label, bool active)
{
    if (config_index < 1 || config_index > 6)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_dio_config_item_t *input = find_input_by_config_index(config_index);
    if (!input)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (label != NULL)
    {
        strncpy(input->label, label, sizeof(input->label) - 1);
    }

    input->active = active;

    ESP_LOGI(TAG, "Updated input %d (port %d): '%s' active=%s",
             config_index, input->port_index, input->label, active ? "yes" : "no");

    return ESP_OK;
}

esp_err_t um_dio_config_update_output(uint8_t config_index, const char *label, bool active, int default_state)
{
    if (config_index < 1 || config_index > 8)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_dio_config_item_t *output = find_output_by_config_index(config_index);
    if (!output)
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (label != NULL)
    {
        strncpy(output->label, label, sizeof(output->label) - 1);
    }

    output->active = active;
    output->default_state = (default_state != 0) ? 1 : 0;

    ESP_LOGI(TAG, "Updated output %d (port %d): '%s' active=%s default=%d",
             config_index, output->port_index, output->label,
             active ? "yes" : "no", output->default_state);

    return ESP_OK;
}

uint8_t um_dio_config_get_input_port(uint8_t config_index)
{
    const um_dio_config_item_t *input = um_dio_config_get_input(config_index);
    return input ? input->port_index : 0;
}

uint8_t um_dio_config_get_output_port(uint8_t config_index)
{
    const um_dio_config_item_t *output = um_dio_config_get_output(config_index);
    return output ? output->port_index : 0;
}

char *um_dio_config_get_inputs_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *inputs_array = cJSON_CreateArray();

    // Добавляем все входы
    for (int i = 0; i < dio_config.input_count; i++)
    {
        // Получаем целевой номер порта, который хотим видеть на этом месте в JSON (2, 3, 1, 0, 5, 4)
        uint8_t target_port = um_dio_get_input_index(i);

        // Ищем в массиве inputs тот элемент, у которого port_index == target_port
        int found_idx = -1;
        for (int j = 0; j < dio_config.input_count; j++)
        {
            if (dio_config.inputs[j].port_index == target_port)
            {
                found_idx = j;
                break;
            }
        }

        if (found_idx != -1)
        {
            bool state = false;
            // Работаем с найденным элементом
            um_dio_get_input(target_port, &state);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "index", dio_config.inputs[found_idx].config_index);
            cJSON_AddNumberToObject(item, "port", dio_config.inputs[found_idx].port_index);
            cJSON_AddStringToObject(item, "label", dio_config.inputs[found_idx].label);
            cJSON_AddBoolToObject(item, "active", dio_config.inputs[found_idx].active);
            cJSON_AddBoolToObject(item, "state", state);
            cJSON_AddItemToArray(inputs_array, item);
        }
    }

    cJSON_AddItemToObject(root, "inputs", inputs_array);
    cJSON_AddNumberToObject(root, "count", dio_config.input_count);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}

char *um_dio_config_get_outputs_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *outputs_array = cJSON_CreateArray();

    // Добавляем все выходы
    for (int i = 0; i < dio_config.output_count; i++)
    {
        bool state = false;
        um_dio_get_output(dio_config.outputs[i].port_index, &state);
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "index", dio_config.outputs[i].config_index);
        cJSON_AddNumberToObject(item, "port", dio_config.outputs[i].port_index);
        cJSON_AddStringToObject(item, "label", dio_config.outputs[i].label);
        cJSON_AddBoolToObject(item, "active", dio_config.outputs[i].active);
        cJSON_AddNumberToObject(item, "default_state", dio_config.outputs[i].default_state);
        cJSON_AddBoolToObject(item, "state", state);
        cJSON_AddItemToArray(outputs_array, item);
    }

    cJSON_AddItemToObject(root, "outputs", outputs_array);
    cJSON_AddNumberToObject(root, "count", dio_config.output_count);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}