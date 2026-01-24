# UMN Events Component (Компонент событий)

Простой компонент шины событий для ESP-IDF приложений.

## Особенности

- Простой API для публикации и подписки на события
- Поддержка пользовательских ID событий
- Опциональные данные событий (payload)
- Потокобезопасная публикация событий
- Легкая интеграция с существующими проектами ESP-IDF

## Установка

1. Скопируйте компонент в директорию `components` вашего проекта:
ваш_проект
```
├── main/
├── components/
│   └── um_events/
│   ├── include/
│   │   └── um_events.h
│   ├── um_events.c
│   └── CMakeLists.txt
└── CMakeLists.txt
```

2. Компонент будет автоматически обнаружен системой сборки ESP-IDF.

## Использование

### 1. Подключите заголовочный файл
```c
#include "um_events.h"
```
2. Инициализируйте шину событий
```c
// В вашей функции app_main() или функции инициализации
ESP_ERROR_CHECK(um_events_init());
```
3. Определите пользовательские ID событий
Можете использовать предопределенные события или добавить свои в um_events.h:

```c
typedef enum {
    // ... существующие события
    UMNI_EVENT_CUSTOM_10 = 10,  // Добавьте свои события
    UMNI_EVENT_CUSTOM_11,
    UMNI_EVENT_SENSOR_READING,   // Чтение сенсора
    UMNI_EVENT_BUTTON_PRESSED,   // Нажатие кнопки
    // ... и т.д.
} umn_event_id_t;
```
4. Подпишитесь на события
```c
// Функция-обработчик события
void my_event_handler(void* handler_arg, 
                     esp_event_base_t event_base, 
                     int32_t event_id, 
                     void* event_data) {
    
    ESP_LOGI("MY_APP", "Получено событие: %ld", (long)event_id);
    
    if (event_data != NULL) {
        // Обработка данных события
        char* message = (char*)event_data;
        ESP_LOGI("MY_APP", "Данные события: %s", message);
    }
}

// Подписка на конкретное событие
ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_CUSTOM_0, 
                                   my_event_handler, 
                                   NULL));

// Подписка на ВСЕ события
ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ANY, 
                                   my_event_handler, 
                                   NULL));
```
5. Публикуйте события
```c
// Публикация события без данных
ESP_ERROR_CHECK(um_event_publish(UMNI_EVENT_CUSTOM_0, 
                                 NULL, 
                                 0, 
                                 portMAX_DELAY));

// Публикация события со строковыми данными
char* message = "Привет из события!";
ESP_ERROR_CHECK(um_event_publish(UMNI_EVENT_CUSTOM_1, 
                                 message, 
                                 strlen(message) + 1, 
                                 portMAX_DELAY));

// Публикация события с пользовательской структурой данных
typedef struct {
    int sensor_value;
    float temperature;
    uint8_t status;
} sensor_data_t;

sensor_data_t data = {
    .sensor_value = 42,
    .temperature = 23.5,
    .status = 0x01
};

ESP_ERROR_CHECK(um_event_publish(UMNI_EVENT_CUSTOM_2, 
                                 &data, 
                                 sizeof(sensor_data_t), 
                                 portMAX_DELAY));
```
6. Отпишитесь от событий
```c
// Когда обработчик больше не нужен
ESP_ERROR_CHECK(um_event_unsubscribe(UMNI_EVENT_CUSTOM_0, 
                                     my_event_handler));
```
Пример приложения
```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "um_events.h"

static const char* TAG = "example";

// Обработчик события 1
void handler1(void* arg, esp_event_base_t base, int32_t id, void* data) {
    ESP_LOGI(TAG, "Handler1: Получено событие %ld", (long)id);
}

// Обработчик события 2
void handler2(void* arg, esp_event_base_t base, int32_t id, void* data) {
    ESP_LOGI(TAG, "Handler2: Получено событие %ld", (long)id);
    
    if (data != NULL) {
        ESP_LOGI(TAG, "Handler2: Данные: %s", (char*)data);
    }
}

void app_main(void) {
    // Инициализация шины событий
    ESP_ERROR_CHECK(um_events_init());
    
    // Подписка обработчиков
    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_CUSTOM_0, handler1, NULL));
    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_CUSTOM_1, handler2, NULL));
    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ANY, handler1, NULL));
    
    // Публикация событий
    ESP_ERROR_CHECK(um_event_publish(UMNI_EVENT_CUSTOM_0, NULL, 0, portMAX_DELAY));
    
    char* message = "Тестовое сообщение";
    ESP_ERROR_CHECK(um_event_publish(UMNI_EVENT_CUSTOM_1, 
                                     message, 
                                     strlen(message) + 1, 
                                     portMAX_DELAY));
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Очистка
    ESP_ERROR_CHECK(um_event_unsubscribe(UMNI_EVENT_CUSTOM_0, handler1));
}
```
Справочник API
`um_events_init()`
Инициализирует шину событий. Должна быть вызвана перед использованием других функций.

`um_event_publish(event_id, event_data, event_data_size, ticks_to_wait)`
Публикует событие в шину.

`um_event_subscribe(event_id, event_handler, handler_arg)`
Подписывает обработчик на событие.

`um_event_unsubscribe(event_id, event_handler)`
Отписывает обработчик от события.

Важные заметки
Данные события копируются при публикации, поэтому их можно освобождать после публикации

Используйте UMNI_EVENT_ANY для подписки на все события этой базы

Шина событий использует стандартный цикл событий ESP-IDF

Все функции потокобезопасны

Компонент работает с ESP-IDF версии 5.5.2 и выше