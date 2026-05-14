## GET
1. Простой пример - вернуть строку
```c
static esp_err_t get_hello(httpd_req_t *req, cJSON **data)
{
    // Создаем JSON объект
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    // Добавляем поля
    cJSON_AddStringToObject(json, "message", "Hello World!");
    cJSON_AddNumberToObject(json, "value", 42);
    
    // Передаем указатель на созданный JSON
    *data = json;
    return ESP_OK;
}

// Регистрация:
um_webserver_register_get("/api/hello", get_hello);
```
2. С параметрами из query string
```c
static esp_err_t get_user(httpd_req_t *req, cJSON **data)
{
    char user_id[32] = {0};
    
    // Получаем параметр id из URL: /api/user?id=123
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0) {
        char *query = malloc(query_len + 1);
        if (query) {
            httpd_req_get_url_query_str(req, query, query_len + 1);
            httpd_query_key_value(query, "id", user_id, sizeof(user_id));
            free(query);
        }
    }
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    cJSON_AddStringToObject(json, "user_id", user_id);
    cJSON_AddStringToObject(json, "name", "Test User");
    cJSON_AddNumberToObject(json, "age", 30);
    
    *data = json;
    return ESP_OK;
}

// Регистрация:
um_webserver_register_get("/api/user", get_user);
```
3. Вернуть массив данных
```c
static esp_err_t get_sensors(httpd_req_t *req, cJSON **data)
{
    // Создаем массив
    cJSON *array = cJSON_CreateArray();
    if (!array) return ESP_ERR_NO_MEM;
    
    // Добавляем элементы
    cJSON *sensor1 = cJSON_CreateObject();
    cJSON_AddStringToObject(sensor1, "name", "Датчик 1");
    cJSON_AddNumberToObject(sensor1, "value", 22.5);
    cJSON_AddItemToArray(array, sensor1);
    
    cJSON *sensor2 = cJSON_CreateObject();
    cJSON_AddStringToObject(sensor2, "name", "Датчик 2");
    cJSON_AddNumberToObject(sensor2, "value", 23.1);
    cJSON_AddItemToArray(array, sensor2);
    
    *data = array;
    return ESP_OK;
}

// Регистрация:
um_webserver_register_get("/api/sensors", get_sensors);
```
4. С проверкой параметров
```c
static esp_err_t get_config(httpd_req_t *req, cJSON **data)
{
    char section[32] = {0};
    
    // Получаем секцию
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0) {
        char *query = malloc(query_len + 1);
        if (query) {
            httpd_req_get_url_query_str(req, query, query_len + 1);
            httpd_query_key_value(query, "section", section, sizeof(section));
            free(query);
        }
    }
    
    // Проверяем обязательный параметр
    if (strlen(section) == 0) {
        return ESP_ERR_INVALID_ARG;  // Базовый обработчик вернет {"success":false,"error":"Invalid arguments"}
    }
    
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    if (strcmp(section, "wifi") == 0) {
        cJSON_AddStringToObject(json, "ssid", "MyWiFi");
        cJSON_AddStringToObject(json, "password", "********");
    } else if (strcmp(section, "system") == 0) {
        cJSON_AddNumberToObject(json, "uptime", esp_timer_get_time() / 1000000);
        cJSON_AddNumberToObject(json, "free_heap", esp_get_free_heap_size());
    } else {
        cJSON_Delete(json);
        return ESP_ERR_NOT_FOUND;  // {"success":false,"error":"Not found"}
    }
    
    *data = json;
    return ESP_OK;
}

um_webserver_register_get("/api/config", get_config);
```
5. Из NVS или файла
```c
static esp_err_t get_onewire_config(httpd_req_t *req, cJSON **data)
{
    // Читаем конфиг из NVS или файла
    char *config_str = um_onewire_config_read();
    if (!config_str) {
        return ESP_FAIL;
    }
    
    // Парсим строку в JSON
    cJSON *json = cJSON_Parse(config_str);
    free(config_str);
    
    if (!json) {
        return ESP_FAIL;
    }
    
    *data = json;
    return ESP_OK;
}

um_webserver_register_get("/api/onewire/config", get_onewire_config);
```
6. С возвратом разных типов данных
```c
static esp_err_t get_status(httpd_req_t *req, cJSON **data)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return ESP_ERR_NO_MEM;
    
    // Разные типы данных
    cJSON_AddStringToObject(json, "status", "online");
    cJSON_AddNumberToObject(json, "uptime", 3600);
    cJSON_AddBoolToObject(json, "wifi_connected", true);
    
    // Вложенный объект
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors, "temperature", 22.5);
    cJSON_AddNumberToObject(sensors, "humidity", 45);
    cJSON_AddItemToObject(json, "sensors", sensors);
    
    // Массив
    cJSON *errors = cJSON_CreateArray();
    cJSON_AddItemToArray(errors, cJSON_CreateString("None"));
    cJSON_AddItemToObject(json, "errors", errors);
    
    *data = json;
    return ESP_OK;
}

um_webserver_register_get("/api/status", get_status);
```
7. Использование в main.c
```c
void app_main(void)
{
    // ... инициализация ...
    
    um_webserver_start();
    
    // Регистрируем все endpoints
    um_webserver_register_get("/api/hello", get_hello);
    um_webserver_register_get("/api/user", get_user);
    um_webserver_register_get("/api/sensors", get_sensors);
    um_webserver_register_get("/api/config", get_config);
    um_webserver_register_get("/api/status", get_status);
    um_webserver_register_get("/api/onewire/config", get_onewire_config);
    
    // ... остальной код ...
}
```
Важно!
В data_func вы НЕ должны:

❌ Отправлять ответ сами (httpd_resp_sendstr)

❌ Устанавливать тип контента

❌ Создавать обертку {"success":...}

Вы только создаете cJSON объект с данными и возвращаете его через *data. Всё остальное делает базовый обработчик!

## POST

1. Простой пример - эхо (вернуть то же, что получили)
```c
static esp_err_t post_echo(httpd_req_t *req, cJSON *input, cJSON **output)
{
    // Просто возвращаем копию входных данных
    *output = cJSON_Duplicate(input, 1);
    return ESP_OK;
}

// Регистрация:
um_webserver_register_post("/api/echo", post_echo);

// Пример запроса:
// POST /api/echo
// {"message": "Hello", "value": 123}
// Ответ: {"success":true,"data":{"message":"Hello","value":123}}
```
2. Логин/аутентификация
```c
static esp_err_t post_login(httpd_req_t *req, cJSON *input, cJSON **output)
{
    // Извлекаем данные из запроса
    cJSON *username = cJSON_GetObjectItem(input, "username");
    cJSON *password = cJSON_GetObjectItem(input, "password");
    
    if (!username || !password || !username->valuestring || !password->valuestring) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Проверка (в реальности - из NVS)
    if (strcmp(username->valuestring, "admin") == 0 && 
        strcmp(password->valuestring, "1234") == 0) {
        
        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "token", "secret_token_12345");
        cJSON_AddNumberToObject(data, "expires_in", 3600);
        
        *output = data;
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND;  // Неверные учетные данные
}

um_webserver_register_post("/api/login", post_login);
```
3. Управление выходом (реле)
```c
static esp_err_t post_set_output(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *pin = cJSON_GetObjectItem(input, "pin");
    cJSON *value = cJSON_GetObjectItem(input, "value");
    
    if (!pin || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int pin_num = pin->valueint;
    bool pin_value = value->valueint != 0;
    
    do_set_level(pin_num, pin_value);
    
    // Возвращаем подтверждение
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "pin", pin_num);
    cJSON_AddBoolToObject(data, "value", pin_value);
    cJSON_AddStringToObject(data, "status", "ok");
    
    *output = data;
    return ESP_OK;
}

um_webserver_register_post("/api/output", post_set_output);
```
4. Сохранение конфигурации
```c
static esp_err_t post_save_config(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *section = cJSON_GetObjectItem(input, "section");
    cJSON *data = cJSON_GetObjectItem(input, "data");
    
    if (!section || !data || !section->valuestring) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Конвертируем data в строку для сохранения
    char *config_str = cJSON_PrintUnformatted(data);
    if (!config_str) {
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = ESP_FAIL;
    
    if (strcmp(section->valuestring, "onewire") == 0) {
        #if UM_FEATURE_ENABLED(ONEWIRE)
        ret = um_onewire_config_write(config_str);
        #endif
    }
    else if (strcmp(section->valuestring, "wifi") == 0) {
        // Сохранить WiFi конфиг
        ret = ESP_OK;
    }
    
    free(config_str);
    
    if (ret != ESP_OK) {
        return ESP_FAIL;
    }
    
    // Возвращаем результат
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "section", section->valuestring);
    cJSON_AddStringToObject(result, "status", "saved");
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/config", post_save_config);
```
5. Пакетное обновление нескольких значений
```c
static esp_err_t post_batch_update(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *updates = cJSON_GetObjectItem(input, "updates");
    
    if (!updates || !cJSON_IsArray(updates)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int success_count = 0;
    int fail_count = 0;
    cJSON *failed_items = cJSON_CreateArray();
    
    // Обрабатываем каждый элемент в массиве
    cJSON *item;
    cJSON_ArrayForEach(item, updates) {
        cJSON *type = cJSON_GetObjectItem(item, "type");
        cJSON *id = cJSON_GetObjectItem(item, "id");
        cJSON *value = cJSON_GetObjectItem(item, "value");
        
        if (!type || !id || !value) {
            fail_count++;
            cJSON_AddItemToArray(failed_items, cJSON_Duplicate(item, 1));
            continue;
        }
        
        if (strcmp(type->valuestring, "output") == 0) {
            // Установить выход
            do_set_level(id->valueint, value->valueint);
            success_count++;
        }
        else if (strcmp(type->valuestring, "config") == 0) {
            // Обновить конфиг
            // ...
            success_count++;
        }
        else {
            fail_count++;
            cJSON_AddItemToArray(failed_items, cJSON_Duplicate(item, 1));
        }
    }
    
    // Формируем ответ
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "success_count", success_count);
    cJSON_AddNumberToObject(result, "fail_count", fail_count);
    cJSON_AddItemToObject(result, "failed", failed_items);
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/batch", post_batch_update);
```
6. Создание нового ресурса (например, добавление датчика)
```c
static esp_err_t post_add_sensor(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *name = cJSON_GetObjectItem(input, "name");
    cJSON *type = cJSON_GetObjectItem(input, "type");
    cJSON *params = cJSON_GetObjectItem(input, "params");
    
    if (!name || !type || !name->valuestring || !type->valuestring) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Генерируем ID для нового датчика
    uint32_t new_id = esp_random();
    
    // Сохраняем в конфиг (здесь упрощенно)
    // ...
    
    // Возвращаем созданный ресурс с ID
    cJSON *result = cJSON_Duplicate(input, 1);
    cJSON_AddNumberToObject(result, "id", new_id);
    cJSON_AddStringToObject(result, "status", "created");
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/sensors", post_add_sensor);
```
7. Выполнение команды на устройстве
```c
static esp_err_t post_command(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *cmd = cJSON_GetObjectItem(input, "command");
    cJSON *params = cJSON_GetObjectItem(input, "params");
    
    if (!cmd || !cmd->valuestring) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "command", cmd->valuestring);
    
    if (strcmp(cmd->valuestring, "restart") == 0) {
        cJSON_AddStringToObject(result, "status", "restarting");
        *output = result;
        
        // Рестарт через секунду
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        return ESP_OK;  // Сюда не дойдем
    }
    else if (strcmp(cmd->valuestring, "scan_wifi") == 0) {
        // Запускаем сканирование WiFi
        cJSON_AddStringToObject(result, "status", "scanning");
        // ... код сканирования
    }
    else if (strcmp(cmd->valuestring, "clear_config") == 0) {
        // Сброс настроек
        um_nvs_erase_all();
        cJSON_AddStringToObject(result, "status", "cleared");
    }
    else {
        cJSON_Delete(result);
        return ESP_ERR_NOT_FOUND;
    }
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/command", post_command);
```
8. Валидация входных данных
```c
static esp_err_t post_update_thresholds(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *sensor = cJSON_GetObjectItem(input, "sensor");
    cJSON *min = cJSON_GetObjectItem(input, "min");
    cJSON *max = cJSON_GetObjectItem(input, "max");
    
    if (!sensor || !sensor->valuestring || !min || !max) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Валидация значений
    float min_val = min->valuedouble;
    float max_val = max->valuedouble;
    
    if (min_val >= max_val) {
        return ESP_ERR_INVALID_ARG;  // min должен быть меньше max
    }
    
    if (min_val < -50 || max_val > 150) {
        return ESP_ERR_INVALID_ARG;  // Выход за допустимые пределы
    }
    
    // Сохраняем пороги
    // ...
    
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "sensor", sensor->valuestring);
    cJSON_AddNumberToObject(result, "min", min_val);
    cJSON_AddNumberToObject(result, "max", max_val);
    cJSON_AddStringToObject(result, "status", "updated");
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/thresholds", post_update_thresholds);
```
9. Загрузка файла конфигурации (JSON)
```c
static esp_err_t post_upload_config(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *config = cJSON_GetObjectItem(input, "config");
    cJSON *overwrite = cJSON_GetObjectItem(input, "overwrite");
    
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    bool overwrite_flag = overwrite ? overwrite->valueint : false;
    
    // Конвертируем в строку
    char *full_config = cJSON_PrintUnformatted(config);
    if (!full_config) {
        return ESP_ERR_NO_MEM;
    }
    
    // Сохраняем в файл или NVS
    FILE *f = fopen("/spiffs/config.json", "w");
    if (f) {
        fprintf(f, "%s", full_config);
        fclose(f);
    }
    
    free(full_config);
    
    // Проверяем что сохранилось
    cJSON *result = cJSON_CreateObject();
    cJSON_AddBoolToObject(result, "success", true);
    cJSON_AddBoolToObject(result, "overwrite", overwrite_flag);
    cJSON_AddNumberToObject(result, "size", cJSON_GetArraySize(config));
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/config/upload", post_upload_config);
```
10. Комбинированный пример с разными типами данных
```c
static esp_err_t post_telemetry(httpd_req_t *req, cJSON *input, cJSON **output)
{
    // Ожидаем: {"device":"sensor1", "values":[22.5, 45, 1013], "timestamp":1234567890}
    
    cJSON *device = cJSON_GetObjectItem(input, "device");
    cJSON *values = cJSON_GetObjectItem(input, "values");
    cJSON *timestamp = cJSON_GetObjectItem(input, "timestamp");
    
    if (!device || !device->valuestring || !values || !cJSON_IsArray(values)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Обрабатываем каждое значение
    int count = cJSON_GetArraySize(values);
    float temperature = 0, humidity = 0, pressure = 0;
    
    if (count > 0) temperature = cJSON_GetArrayItem(values, 0)->valuedouble;
    if (count > 1) humidity = cJSON_GetArrayItem(values, 1)->valuedouble;
    if (count > 2) pressure = cJSON_GetArrayItem(values, 2)->valuedouble;
    
    // Сохраняем в историю (пример)
    // save_to_influxdb(device->valuestring, temperature, humidity, pressure, timestamp->valuedouble);
    
    // Возвращаем статистику
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "device", device->valuestring);
    cJSON_AddNumberToObject(result, "received", count);
    cJSON_AddNumberToObject(result, "timestamp", timestamp ? timestamp->valuedouble : 0);
    cJSON_AddStringToObject(result, "status", "stored");
    
    *output = result;
    return ESP_OK;
}

um_webserver_register_post("/api/telemetry", post_telemetry);
```
Важно для POST:
В process_func вы:

✅ Получаете input - уже распарсенный JSON от клиента

✅ Создаете output - данные для ответа (или NULL)

✅ Возвращаете ESP_OK при успехе или код ошибки

Базовый обработчик сам:

📦 Читает тело запроса

🔍 Парсит JSON

📤 Отправляет ответ с правильной структурой

🧹 Очищает память

Пример ответа при успехе:

json
{"success":true,"data":{...}}
Пример ответа при ошибке:

json
{"success":false,"error":"Invalid arguments"}