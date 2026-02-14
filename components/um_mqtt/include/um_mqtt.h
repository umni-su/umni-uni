#ifndef UM_MQTT_H
#define UM_MQTT_H

#include "esp_err.h"
#include "mqtt_client.h"
#include "base_config.h"

#if UM_FEATURE_ENABLED(MQTT)

#define UM_MQTT_REGISTER_TIMEOUT 30000         // 30 секунд
#define UM_MQTT_RECONNECT_CHECK_INTERVAL 30000 // 30 секунд

// Базовые топики
#define UM_MQTT_TOPIC_PREFIX_MANAGE "manage/"
#define UM_MQTT_TOPIC_PREFIX_DEVICE "device/"
#define UM_MQTT_TOPIC_REGISTER "/register"
#define UM_MQTT_TOPIC_STATUS "/status"
#define UM_MQTT_TOPIC_LWT "/lwt"
#define UM_MQTT_TOPIC_PING "/ping"
#define UM_MQTT_TOPIC_PONG "/pong"
#define UM_MQTT_TOPIC_SUBSCRIBE "/subscribe"
#define UM_MQTT_TOPIC_CONFIG "/config"

// Структура статуса подключения
typedef struct
{
    bool connected;
    char *broker_url;
    uint16_t broker_port;
    char *client_id;
    bool enabled;
    esp_mqtt_client_handle_t client;
} um_mqtt_status_t;

// Коллбэк для обработки входящих сообщений
typedef void (*um_mqtt_data_callback_t)(const char *topic, const char *data, int data_len);

/**
 * @brief Инициализация MQTT клиента с параметрами из NVS
 * @param client_id Идентификатор клиента (обычно MAC/имя устройства)
 */
void um_mqtt_init(const char *client_id);

/**
 * @brief Деинициализация MQTT клиента
 */
void um_mqtt_deinit(void);

/**
 * @brief Получить статус подключения
 * @return um_mqtt_status_t Структура со статусом
 */
um_mqtt_status_t um_mqtt_get_status(void);

/**
 * @brief Опубликовать данные в топик
 * @param topic Топик (будет автоматически дополнен префиксом device/{client_id})
 * @param data Данные для публикации
 * @param qos QoS (0, 1 или 2)
 * @param retain Retain флаг
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t um_mqtt_publish(const char *topic, const char *data, int qos, int retain);

/**
 * @brief Опубликовать данные в полный топик (без автоматического префикса)
 * @param full_topic Полный топик
 * @param data Данные для публикации
 * @param qos QoS (0, 1 или 2)
 * @param retain Retain флаг
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t um_mqtt_publish_full(const char *full_topic, const char *data, int qos, int retain);

/**
 * @brief Подписаться на топик
 * @param topic Топик для подписки
 * @param qos QoS (0 или 1)
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t um_mqtt_subscribe(const char *topic, int qos);

/**
 * @brief Подписаться на полный топик
 * @param full_topic Полный топик для подписки
 * @param qos QoS (0 или 1)
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t um_mqtt_subscribe_full(const char *full_topic, int qos);

/**
 * @brief Отписаться от топика
 * @param topic Топик для отписки
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t um_mqtt_unsubscribe(const char *topic);

/**
 * @brief Зарегистрировать устройство (публикация в топик /register)
 * @param device_type Тип устройства (опционально)
 * @return esp_err_t ESP_OK при успехе
 */
esp_err_t um_mqtt_register_device(const char *device_type);

/**
 * @brief Зарегистрировать коллбэк для обработки входящих данных
 * @param callback Функция коллбэк
 */
void um_mqtt_set_data_callback(um_mqtt_data_callback_t callback);

/**
 * @brief Принудительное переподключение к брокеру
 */
void um_mqtt_reconnect(void);

/**
 * @brief Получить полный топик с префиксом device/{client_id}
 * @param topic Исходный топик
 * @param buffer Буфер для результата
 * @param buffer_size Размер буфера
 * @return char* Указатель на buffer или NULL при ошибке
 */
char *um_mqtt_get_device_topic(const char *topic, char *buffer, size_t buffer_size);

/**
 * @brief Обновить конфигурацию MQTT из NVS
 * @return true если конфигурация изменилась и требуется перезапуск
 */
bool um_mqtt_update_config(void);

#else // UM_FEATURE_ENABLED(MQTT)

// Пустые заглушки когда MQTT отключен
#define um_mqtt_init(client_id) \
    do                          \
    {                           \
    } while (0)
#define um_mqtt_deinit() \
    do                   \
    {                    \
    } while (0)
#define um_mqtt_get_status() (um_mqtt_status_t){0}
#define um_mqtt_publish(topic, data, qos, retain) ESP_ERR_NOT_SUPPORTED
#define um_mqtt_publish_full(full_topic, data, qos, retain) ESP_ERR_NOT_SUPPORTED
#define um_mqtt_subscribe(topic, qos) ESP_ERR_NOT_SUPPORTED
#define um_mqtt_subscribe_full(full_topic, qos) ESP_ERR_NOT_SUPPORTED
#define um_mqtt_unsubscribe(topic) ESP_ERR_NOT_SUPPORTED
#define um_mqtt_register_device(device_type) ESP_ERR_NOT_SUPPORTED
#define um_mqtt_set_data_callback(callback) \
    do                                      \
    {                                       \
    } while (0)
#define um_mqtt_reconnect() \
    do                      \
    {                       \
    } while (0)
#define um_mqtt_get_device_topic(topic, buffer, size) NULL
#define um_mqtt_update_config() false

#endif // UM_FEATURE_ENABLED(MQTT)

#endif // UM_MQTT_H