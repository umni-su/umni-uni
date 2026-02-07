```c
#include "um_onewire.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

static const char *TAG = "onewire_example";

void onewire_example_task(void *pvParameter) {
    // Инициализируем шину 1-Wire
    esp_err_t ret = um_onewire_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize 1-Wire bus");
        vTaskDelete(NULL);
        return;
    }
    
    // Сканируем датчики
    uint8_t sensor_count = um_onewire_scan();
    ESP_LOGI(TAG, "Found %d sensors", sensor_count);
    
    if (sensor_count > 0) {
        // Получаем состояние шины
        const um_onewire_state_t* state = um_onewire_get_state();
        
        // Выводим информацию о датчиках
        for (uint8_t i = 0; i < sensor_count; i++) {
            const um_onewire_sensor_t* sensor = um_onewire_get_sensor(i);
            if (sensor) {
                ESP_LOGI(TAG, "Sensor %d: %s (%s)", 
                        i, sensor->serial, 
                        um_onewire_sensor_type_to_string(sensor->type));
            }
        }
        
        // Основной цикл чтения температуры
        while (1) {
            ESP_LOGI(TAG, "Reading temperatures...");
            
            // Читаем температуру со всех датчиков
            ret = um_onewire_read_all_temperatures();
            if (ret == ESP_OK) {
                // Выводим значения
                for (uint8_t i = 0; i < sensor_count; i++) {
                    const um_onewire_sensor_t* sensor = um_onewire_get_sensor(i);
                    if (sensor) {
                        ESP_LOGI(TAG, "%s: %.2f°C", sensor->serial, sensor->temperature);
                    }
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(5000)); // Ждем 5 секунд
        }
    }
    
    vTaskDelete(NULL);
}

void app_main() {
    xTaskCreate(onewire_example_task, "onewire_example", 4096, NULL, 5, NULL);
}
```