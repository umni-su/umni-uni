🚀 Использование
1. Инициализация
```c
#include "um_capabilities.h"

void app_main(void)
{
    // Обязательно вызвать перед использованием
    um_capabilities_init();
    
    // ... остальной код
}
```
2. Проверка фич
```c
// Проверка конкретной фичи
if (um_capabilities_has(UM_CAP_WIFI)) {
    // WiFi доступен
}

// Проверка по маске (быстрый способ)
uint64_t network_mask = CAP_MASK(UM_CAP_ETHERNET) | CAP_MASK(UM_CAP_WIFI);
if (um_capabilities_has_any(network_mask)) {
    // Есть хотя бы один сетевой интерфейс
}

// Получить общую маску
uint64_t all_enabled = um_capabilities_get_mask();

// Количество включенных фич
uint32_t count = um_capabilities_get_count();
```
3. Получение JSON для веб-сервера
```c
// Как массив ["wifi", "ethernet", ...]
char *json_array = um_capabilities_get_json_array();
httpd_resp_send(req, json_array, strlen(json_array));
free(json_array);

// Как объект {"wifi":true, "ethernet":true, ...}
char *json_object = um_capabilities_get_json_object();
httpd_resp_send(req, json_object, strlen(json_object));
free(json_object);
```
4. Получение имени фичи
```c
const char *name = um_capabilities_get_name(UM_CAP_WIFI);
// name = "wifi"
```


⚠️ Важно
Всегда вызывайте `um_capabilities_init()` перед использованием

JSON строки нужно освобождать через `free()`

Макрос `CAP_MASK(cap)` доступен для создания масок

`base_config.h` должен определять UM_FEATURE_ENABLED

🎯 Примеры использования
Веб-сервер
```c
esp_err_t capabilities_handler(httpd_req_t *req)
{
    char *json = um_capabilities_get_json_object();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    
    return ESP_OK;
}
```
Валидация команд
```c
bool validate_command(const char *cmd)
{
    if (strcmp(cmd, "read_adc") == 0) {
        return um_capabilities_has(UM_CAP_ADC);
    }
    if (strcmp(cmd, "set_out1") == 0) {
        return um_capabilities_has(UM_CAP_OUT1);
    }
    return false;
}
```
Групповые проверки
```c
#define SENSOR_MASK (CAP_MASK(UM_CAP_ADC) | CAP_MASK(UM_CAP_NTC1) | CAP_MASK(UM_CAP_NTC2))

void read_all_sensors(void)
{
    if (!um_capabilities_has_any(SENSOR_MASK)) {
        printf("No sensors available\n");
        return;
    }
    
    if (um_capabilities_has(UM_CAP_ADC)) {
        // читаем ADC
    }
    // ...
}
```