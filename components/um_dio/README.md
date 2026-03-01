# UMNI DIO Component
Digital Input/Output component for PCF8574 in ESP-IDF projects.

Features
- Output Control: Up to 8 digital outputs with persistent storage in NVS
- Input Monitoring: Up to 6 digital inputs with interrupt-based change detection
- Flexible Configuration: Each I/O can be individually enabled/disabled via Kconfig or sdkconfig.defaults
- Custom Pin Mapping: Flexible mapping between logical channels and physical PCF8574 pins

## API Reference

Initialization

`um_dio_init()` - Initialize the DIO module

`um_dio_deinit()` - Clean up resources

Output Functions

`um_dio_set_output(channel, state)` - Set single output

`um_dio_get_output(channel, &state)` - Read single output state

`um_dio_toggle_output(channel)` - Toggle output

`um_dio_set_all_outputs(bitmask)` - Set all outputs

`um_dio_get_all_outputs(&bitmask)` - Get all outputs state

Input Functions

`um_dio_get_input(channel, &state)` - Read single input

`um_dio_get_all_inputs(&bitmask)` - Get all inputs state

```c
// Application code:
void monitor_inputs(void)
{
    uint8_t prev_inputs = 0;
    
    while (1) {
        uint8_t current_inputs;
        um_dio_get_all_inputs(&current_inputs);
        
        if (current_inputs != prev_inputs) {
            printf("Inputs changed: 0x%02X -> 0x%02X\n", 
                   prev_inputs, current_inputs);
            prev_inputs = current_inputs;
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
```


# Конфигурация

## 1. Инициализация и загрузка конфигурации

```c
#include "um_dio_config.h"

// Где-то при старте системы
void app_main(void)
{
    // Просто загружаем конфигурацию
    // Если файла нет - создастся автоматически с дефолтными значениями
    esp_err_t err = um_dio_config_load();
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to load DIO config");
    }
}
```
## 2. Получение конфигурации каналов

```c
// Получить конфигурацию входа
const um_dio_config_item_t *input = um_dio_config_get_input(0);
if (input) {
    printf("Input 0: label=%s, active=%s\n", 
           input->label, 
           input->active ? "yes" : "no");
}

// Получить конфигурацию выхода
const um_dio_config_item_t *output = um_dio_config_get_output(2);
if (output) {
    printf("Output 2: label=%s, active=%s, default=%d\n", 
           output->label, 
           output->active ? "yes" : "no",
           output->default_state);
}
```
## 3. Обновление конфигурации

```c
// Обновить вход
esp_err_t err = um_dio_config_update_input(
    0,                    // индекс входа
    "Front door sensor",  // новое название (можно NULL)
    true                  // активен
);

// Обновить выход
err = um_dio_config_update_output(
    1,                    // индекс выхода
    "Garage light",       // новое название (можно NULL)
    true,                 // активен
    0                     // состояние по умолчанию (0 или 1)
);

// После всех изменений - сохранить в файл
if (err == ESP_OK) {
    um_dio_config_save();
}
```
## 4. Чтение конфигурации как JSON строки
```c
// Получить JSON строку (нужно освободить через free!)
char *json_str = um_dio_config_read();
if (json_str) {
    printf("Current config: %s\n", json_str);
    free(json_str);  // ВАЖНО: всегда освобождать!
}
```
## 5. Принудительное создание конфигурации по умолчанию
```c
// Если нужно сбросить конфигурацию на дефолтную
esp_err_t err = um_dio_config_create_default();
if (err == ESP_OK) {
    printf("Default config created\n");
}
```
## 6. Полный пример использования
```c
#include "um_dio_config.h"
#include "um_capabilities.h"

void dio_config_example(void)
{
    // 1. Загружаем конфигурацию (создастся автоматически если нет файла)
    ESP_ERROR_CHECK(um_dio_config_load());
    
    // 2. Получаем и выводим все активные входы
    printf("\n=== Inputs ===\n");
    for (int i = 0; i < 6; i++) {
        const um_dio_config_item_t *input = um_dio_config_get_input(i);
        if (input && input->active) {
            printf("IN%d: %s\n", i + 1, input->label);
        }
    }
    
    // 3. Получаем и выводим все активные выходы
    printf("\n=== Outputs ===\n");
    for (int i = 0; i < 8; i++) {
        const um_dio_config_item_t *output = um_dio_config_get_output(i);
        if (output && output->active) {
            printf("OUT%d: %s (default: %d)\n", 
                   i + 1, output->label, output->default_state);
        }
    }
    
    // 4. Обновляем конфигурацию первого выхода
    printf("\nUpdating output 0...\n");
    um_dio_config_update_output(0, "Main light", true, 1);
    
    // 5. Сохраняем изменения
    um_dio_config_save();
    
    // 6. Читаем сохраненный файл
    char *json = um_dio_config_read();
    if (json) {
        printf("\nSaved config:\n%s\n", json);
        free(json);
    }
}
```
## 7. Интеграция с веб-интерфейсом
```c
// HTTP обработчик для получения конфигурации
esp_err_t dio_config_get_handler(httpd_req_t *req)
{
    char *json = um_dio_config_read();
    if (!json) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);  // Не забываем освободить!
    
    return ESP_OK;
}

// HTTP обработчик для обновления конфигурации
esp_err_t dio_config_post_handler(httpd_req_t *req)
{
    char buf[256];
    httpd_req_recv(req, buf, sizeof(buf));
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    // Парсим и обновляем конфигурацию
    cJSON *input = cJSON_GetObjectItem(root, "input");
    if (input) {
        int index = cJSON_GetObjectItem(input, "index")->valueint;
        const char *label = cJSON_GetObjectItem(input, "label")->valuestring;
        bool active = cJSON_IsTrue(cJSON_GetObjectItem(input, "active"));
        
        um_dio_config_update_input(index, label, active);
    }
    
    cJSON_Delete(root);
    um_dio_config_save();
    
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}
```