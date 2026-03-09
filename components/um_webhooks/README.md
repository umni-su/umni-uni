# UM Webhooks Component

Простой и легкий компонент для работы с вебхуками в ESP-IDF проектах. Поддерживает HTTP и HTTPS протоколы.

## Возможности

- ✅ Поддержка HTTP и HTTPS
- ✅ Проверка статуса вебхуков (включен/выключен)
- ✅ Автоматическое получение URL из NVS
- ✅ Отправка JSON (cJSON или строка)
- ✅ GET и POST запросы
- ✅ Минимальное потребление памяти
- ✅ Подробное логирование

## Зависимости

- ESP-IDF v4.4 или выше
- Компонент `um_nvs` для хранения конфигурации
- `cJSON` (встроен в ESP-IDF)

## Установка

1. Скопируйте компонент в папку `components/um_webhooks` вашего проекта
2. Добавьте зависимости в `CMakeLists.txt` проекта:

```cmake
set(EXTRA_COMPONENT_DIRS components/um_webhooks)

## Использование

```c

Базовый пример
c
#include "um_webhooks.h"

void app_main(void)
{
    // Проверяем статус
    if (um_webhooks_is_enabled()) {
        printf("Webhooks are enabled!\n");
        
        // Получаем URL
        char *url = um_webhooks_get_url();
        if (url) {
            printf("Webhook URL: %s\n", url);
            free(url);
        }
    }
}

// Простой GET
um_webhooks_get(NULL);

// GET с параметрами
um_webhooks_get("?status=online&uptime=3600");

// GET с HTTPS
// URL должен быть сохранен как https://example.com/webhook
um_webhooks_get("?action=ping");

//Отправка строки напрямую
um_webhooks_post_string("{\"message\":\"Hello, World!\"}");

Отправка простого уведомления
c
// Создаем JSON
cJSON *data = cJSON_CreateObject();
cJSON_AddStringToObject(data, "event", "device_startup");
cJSON_AddNumberToObject(data, "uptime", seconds);
cJSON_AddStringToObject(data, "status", "online");

// Отправляем
esp_err_t err = um_webhooks_post_json(data);
if (err == ESP_OK) {
    printf("Webhook sent successfully\n");
}

// Очищаем память
cJSON_Delete(data);

// Отправка данных сенсора
void send_sensor_data(float temperature, float humidity)
{
    cJSON *data = cJSON_CreateObject();
    cJSON *sensor = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(sensor, "temp", temperature);
    cJSON_AddNumberToObject(sensor, "hum", humidity);
    cJSON_AddItemToObject(data, "sensor", sensor);
    cJSON_AddStringToObject(data, "device_id", "esp32_01");
    
    um_webhooks_post_json(data);
    cJSON_Delete(data);
}
```