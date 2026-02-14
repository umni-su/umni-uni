```c
// main.c
#include "um_mqtt.h"

// Коллбэк для обработки входящих сообщений
void my_mqtt_callback(const char* topic, const char* data, int data_len) {
    ESP_LOGI("app", "Received on %s: %.*s", topic, data_len, data);
    
    // Пример обработки команд
    if (strstr(topic, "/command") != NULL) {
        // Выполнить команду
    }
}

void app_main(void) {
    // Инициализация MQTT
    um_mqtt_init("test.mosquitto.org", 1883, "my_device_001", NULL, NULL);
    
    // Установка коллбэка
    um_mqtt_set_data_callback(my_mqtt_callback);
    
    // Подписка на топики
    um_mqtt_subscribe("/command", 1);
    um_mqtt_subscribe("/config", 1);
    
    // Публикация статуса
    um_mqtt_publish("/status", "{\"temp\":23.5}", 0, 0);
    
    // Регистрация устройства
    um_mqtt_register_device("sensor");
    
    // Получение статуса
    um_mqtt_status_t status = um_mqtt_get_status();
    ESP_LOGI("app", "MQTT connected: %d", status.connected);
}
```