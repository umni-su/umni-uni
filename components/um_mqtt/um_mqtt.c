#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "base_config.h"

#if UM_FEATURE_ENABLED(MQTT)

#include "um_mqtt.h"
#include "um_nvs.h"

static const char *TAG = "um_mqtt";

// Структура состояния MQTT клиента
typedef struct
{
    esp_mqtt_client_handle_t client;
    char *broker_url;
    char *client_id;
    char *username;
    char *password;
    uint16_t port;
    bool connected;
    bool initialized;
    bool enabled;
    bool config_changed;
    um_mqtt_data_callback_t data_callback;
    TaskHandle_t register_task;
    TaskHandle_t check_task;
} mqtt_state_t;

static mqtt_state_t mqtt_state = {
    .client = NULL,
    .broker_url = NULL,
    .client_id = NULL,
    .username = NULL,
    .password = NULL,
    .port = 1883,
    .connected = false,
    .initialized = false,
    .enabled = false,
    .config_changed = false,
    .data_callback = NULL,
    .register_task = NULL,
    .check_task = NULL};

// Вспомогательная функция для логирования свободной памяти
static void log_free_heap(const char *function_name)
{
    ESP_LOGW(TAG, "[%s] Free memory: %ld bytes",
             function_name, esp_get_free_heap_size());
}

// Получение LWT топика
static char *get_lwt_topic(char *buffer, size_t buffer_size)
{
    if (!mqtt_state.client_id)
        return NULL;

    snprintf(buffer, buffer_size, "%s%s%s",
             UM_MQTT_TOPIC_PREFIX_DEVICE, mqtt_state.client_id, UM_MQTT_TOPIC_LWT);
    return buffer;
}

// Загрузка конфигурации из NVS
static esp_err_t load_config_from_nvs(void)
{
    bool enabled = false;
    char *host = NULL;
    uint16_t port = 1883;
    char *username = NULL;
    char *password = NULL;

    // Читаем настройки из NVS
    um_nvs_get_mqtt_enabled(&enabled);
    um_nvs_get_mqtt_host(&host);
    um_nvs_get_mqtt_port(&port);
    um_nvs_get_mqtt_username(&username);
    um_nvs_get_mqtt_password(&password);

    // Проверяем, изменилась ли конфигурация
    bool changed = false;

    if (mqtt_state.enabled != enabled)
    {
        mqtt_state.enabled = enabled;
        changed = true;
    }

    if (mqtt_state.port != port)
    {
        mqtt_state.port = port;
        changed = true;
    }

    if (host)
    {
        if (!mqtt_state.broker_url || strcmp(mqtt_state.broker_url, host) != 0)
        {
            if (mqtt_state.broker_url)
                free(mqtt_state.broker_url);
            mqtt_state.broker_url = strdup(host);
            changed = true;
        }
        free(host);
    }

    if (username)
    {
        if (!mqtt_state.username || strcmp(mqtt_state.username, username) != 0)
        {
            if (mqtt_state.username)
                free(mqtt_state.username);
            mqtt_state.username = strdup(username);
            changed = true;
        }
        free(username);
    }
    else
    {
        if (mqtt_state.username)
        {
            free(mqtt_state.username);
            mqtt_state.username = NULL;
            changed = true;
        }
    }

    if (password)
    {
        if (!mqtt_state.password || strcmp(mqtt_state.password, password) != 0)
        {
            if (mqtt_state.password)
                free(mqtt_state.password);
            mqtt_state.password = strdup(password);
            changed = true;
        }
        free(password);
    }
    else
    {
        if (mqtt_state.password)
        {
            free(mqtt_state.password);
            mqtt_state.password = NULL;
            changed = true;
        }
    }

    mqtt_state.config_changed = changed;
    return ESP_OK;
}

// Освобождение ресурсов состояния
static void free_state_resources(void)
{
    if (mqtt_state.broker_url)
    {
        free(mqtt_state.broker_url);
        mqtt_state.broker_url = NULL;
    }
    if (mqtt_state.client_id)
    {
        free(mqtt_state.client_id);
        mqtt_state.client_id = NULL;
    }
    if (mqtt_state.username)
    {
        free(mqtt_state.username);
        mqtt_state.username = NULL;
    }
    if (mqtt_state.password)
    {
        free(mqtt_state.password);
        mqtt_state.password = NULL;
    }
}

// Задача периодической регистрации
static void mqtt_register_task(void *arg)
{
    TickType_t last_register_time = 0;

    while (mqtt_state.initialized && mqtt_state.enabled)
    {
        if (!mqtt_state.connected)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_register_time) > pdMS_TO_TICKS(UM_MQTT_REGISTER_TIMEOUT))
        {
            um_mqtt_register_device("generic");
            last_register_time = current_time;
            log_free_heap(__FUNCTION__);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    mqtt_state.register_task = NULL;
    vTaskDelete(NULL);
}

// Обработчик событий MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        mqtt_state.connected = true;
        ESP_LOGI(TAG, "Connected to MQTT broker: %s:%d",
                 mqtt_state.broker_url, mqtt_state.port);

        // Отправляем LWT online
        char lwt_topic[128];
        if (get_lwt_topic(lwt_topic, sizeof(lwt_topic)))
        {
            esp_mqtt_client_publish(mqtt_state.client, lwt_topic,
                                    "online", 6, 1, 1);
        }

        // Создаем задачу регистрации
        if (mqtt_state.register_task == NULL && mqtt_state.enabled)
        {
            xTaskCreatePinnedToCore(mqtt_register_task, "mqtt_reg",
                                    4096, NULL, 5,
                                    &mqtt_state.register_task, 1);
        }

        log_free_heap(__FUNCTION__);
        break;

    case MQTT_EVENT_DISCONNECTED:
        mqtt_state.connected = false;
        ESP_LOGW(TAG, "Disconnected from MQTT broker");

        if (mqtt_state.register_task)
        {
            vTaskDelete(mqtt_state.register_task);
            mqtt_state.register_task = NULL;
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed successfully, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "Unsubscribed successfully, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Published successfully, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
    {
        // Извлекаем данные
        char topic[128] = {0};
        char data[512] = {0};

        int topic_len = event->topic_len < sizeof(topic) - 1 ? event->topic_len : sizeof(topic) - 1;
        int data_len = event->data_len < sizeof(data) - 1 ? event->data_len : sizeof(data) - 1;

        memcpy(topic, event->topic, topic_len);
        memcpy(data, event->data, data_len);

        ESP_LOGI(TAG, "Received data: topic=%s, data=%s", topic, data);

        // Обработка ping
        if (strstr(topic, UM_MQTT_TOPIC_PING) != NULL)
        {
            um_mqtt_publish_full(UM_MQTT_TOPIC_PONG, "pong", 0, 0);
        }

        // Вызов пользовательского коллбэка
        if (mqtt_state.data_callback)
        {
            mqtt_state.data_callback(topic, data, data_len);
        }
        break;
    }

    case MQTT_EVENT_ERROR:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

        if (event->error_handle)
        {
            ESP_LOGI(TAG, "Error type: %d", event->error_handle->error_type);

            switch (event->error_handle->error_type)
            {
            case MQTT_ERROR_TYPE_TCP_TRANSPORT:
                if (event->error_handle->esp_transport_sock_errno != 0)
                {
                    ESP_LOGE(TAG, "Socket error: %d, %s",
                             event->error_handle->esp_transport_sock_errno,
                             strerror(event->error_handle->esp_transport_sock_errno));
                }
                if (event->error_handle->esp_tls_last_esp_err != 0)
                {
                    ESP_LOGE(TAG, "TLS ESP error: 0x%x",
                             event->error_handle->esp_tls_last_esp_err);
                }
                if (event->error_handle->esp_tls_stack_err != 0)
                {
                    ESP_LOGE(TAG, "TLS stack error: 0x%x",
                             event->error_handle->esp_tls_stack_err);
                }
                break;

            case MQTT_ERROR_TYPE_CONNECTION_REFUSED:
                ESP_LOGE(TAG, "Connection refused, return code: %d",
                         event->error_handle->connect_return_code);

                switch (event->error_handle->connect_return_code)
                {
                case MQTT_CONNECTION_ACCEPTED:
                    ESP_LOGW(TAG, "Unexpected: connection accepted but error reported");
                    break;

                case MQTT_CONNECTION_REFUSE_PROTOCOL:
                    ESP_LOGE(TAG, "Refused: unacceptable protocol version");
                    break;

                case MQTT_CONNECTION_REFUSE_ID_REJECTED:
                    ESP_LOGE(TAG, "Refused: identifier rejected");
                    break;

                case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
                    ESP_LOGE(TAG, "Refused: server unavailable");
                    break;

                case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
                    ESP_LOGE(TAG, "Refused: bad username");
                    break;

                case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
                    ESP_LOGE(TAG, "Refused: not authorized (wrong username/password)");
                    break;

                default:
                    ESP_LOGE(TAG, "Unknown connection refusal code: %d",
                             event->error_handle->connect_return_code);
                    break;
                }
                break;
            case MQTT_ERROR_TYPE_SUBSCRIBE_FAILED:
                ESP_LOGE(TAG, "Subscribe failed");
                break;

            default:
                ESP_LOGE(TAG, "Unknown error type: %d",
                         event->error_handle->error_type);
                break;
            }
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Unhandled event id: %d", (int)event_id);
        break;
    }

    log_free_heap(__FUNCTION__);
}

// Реализация публичных функций
void um_mqtt_init(const char *client_id)
{
    if (!client_id)
    {
        ESP_LOGE(TAG, "Invalid parameter: client_id required");
        return;
    }

    // Загружаем конфигурацию из NVS
    load_config_from_nvs();

    // Проверяем, включен ли MQTT
    if (!mqtt_state.enabled)
    {
        ESP_LOGI(TAG, "MQTT is disabled in NVS");
        mqtt_state.initialized = true; // Помечаем как инициализированный для задач мониторинга
    }

    // Сохраняем client_id
    if (mqtt_state.client_id)
    {
        free(mqtt_state.client_id);
    }
    mqtt_state.client_id = strdup(client_id);

    // Если MQTT выключен или нет хоста, не запускаем клиента
    if (!mqtt_state.enabled || !mqtt_state.broker_url)
    {
        ESP_LOGI(TAG, "MQTT not started: enabled=%d, broker=%s",
                 mqtt_state.enabled, mqtt_state.broker_url ? mqtt_state.broker_url : "NULL");
        mqtt_state.initialized = true;
        return;
    }

    // Формируем URI
    char uri[256];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", mqtt_state.broker_url, mqtt_state.port);

    // Получаем LWT топик
    char lwt_topic[128];
    get_lwt_topic(lwt_topic, sizeof(lwt_topic));

    // Конфигурация MQTT клиента
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = uri,
            .address.port = mqtt_state.port,
        },
        .credentials = {
            .username = mqtt_state.username,
            .client_id = mqtt_state.client_id,
            .authentication.password = mqtt_state.password,
        },
        .session = {.keepalive = 30, .disable_clean_session = 0, .last_will = {.topic = lwt_topic, .msg = "offline", .msg_len = 7, .qos = 1, .retain = 1}},
        .network = {
            .reconnect_timeout_ms = 10000,
            .timeout_ms = 10000,
            .disable_auto_reconnect = false,
        },
        .task = {.stack_size = 6144, .priority = 5}};

    // Создаем клиента
    mqtt_state.client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_state.client)
    {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return;
    }

    // Регистрируем обработчик событий
    esp_mqtt_client_register_event(mqtt_state.client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    // Запускаем клиента
    esp_err_t err = esp_mqtt_client_start(mqtt_state.client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(mqtt_state.client);
        mqtt_state.client = NULL;
        return;
    }

    mqtt_state.initialized = true;
    ESP_LOGI(TAG, "MQTT initialized with broker: %s:%d, client_id: %s, enabled: %d",
             mqtt_state.broker_url, mqtt_state.port, client_id, mqtt_state.enabled);

    log_free_heap(__FUNCTION__);
}

void um_mqtt_deinit(void)
{
    if (!mqtt_state.initialized)
        return;

    // Останавливаем задачи
    if (mqtt_state.register_task)
    {
        vTaskDelete(mqtt_state.register_task);
        mqtt_state.register_task = NULL;
    }

    if (mqtt_state.check_task)
    {
        vTaskDelete(mqtt_state.check_task);
        mqtt_state.check_task = NULL;
    }

    // Отправляем offline LWT если были подключены
    if (mqtt_state.connected && mqtt_state.client)
    {
        char lwt_topic[128];
        if (get_lwt_topic(lwt_topic, sizeof(lwt_topic)))
        {
            esp_mqtt_client_publish(mqtt_state.client, lwt_topic,
                                    "offline", 7, 1, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Останавливаем и уничтожаем клиента
    if (mqtt_state.client)
    {
        esp_mqtt_client_stop(mqtt_state.client);
        esp_mqtt_client_destroy(mqtt_state.client);
        mqtt_state.client = NULL;
    }

    // Освобождаем память
    free_state_resources();

    mqtt_state.connected = false;
    mqtt_state.initialized = false;
    mqtt_state.enabled = false;

    ESP_LOGI(TAG, "MQTT deinitialized");
    log_free_heap(__FUNCTION__);
}

um_mqtt_status_t um_mqtt_get_status(void)
{
    um_mqtt_status_t status = {
        .connected = mqtt_state.connected,
        .broker_url = mqtt_state.broker_url,
        .broker_port = mqtt_state.port,
        .client_id = mqtt_state.client_id,
        .enabled = mqtt_state.enabled,
        .client = mqtt_state.client};
    return status;
}

char *um_mqtt_get_device_topic(const char *topic, char *buffer, size_t buffer_size)
{
    if (!topic || !buffer || !mqtt_state.client_id)
        return NULL;

    snprintf(buffer, buffer_size, "%s%s%s",
             UM_MQTT_TOPIC_PREFIX_DEVICE, mqtt_state.client_id, topic);
    return buffer;
}

esp_err_t um_mqtt_publish(const char *topic, const char *data, int qos, int retain)
{
    if (!topic || !data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mqtt_state.enabled || !mqtt_state.connected || !mqtt_state.client)
    {
        ESP_LOGW(TAG, "Cannot publish: enabled=%d, connected=%d, client=%p",
                 mqtt_state.enabled, mqtt_state.connected, mqtt_state.client);
        return ESP_FAIL;
    }

    char full_topic[128];
    if (!um_mqtt_get_device_topic(topic, full_topic, sizeof(full_topic)))
    {
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_state.client, full_topic,
                                         data, 0, qos, retain);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to publish to %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published to %s: %s", full_topic, data);
    log_free_heap(__FUNCTION__);
    return ESP_OK;
}

esp_err_t um_mqtt_publish_full(const char *full_topic, const char *data, int qos, int retain)
{
    if (!full_topic || !data)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mqtt_state.enabled || !mqtt_state.connected || !mqtt_state.client)
    {
        ESP_LOGW(TAG, "Cannot publish: enabled=%d, connected=%d, client=%p",
                 mqtt_state.enabled, mqtt_state.connected, mqtt_state.client);
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_state.client, full_topic,
                                         data, 0, qos, retain);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to publish to %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published to %s: %s", full_topic, data);
    return ESP_OK;
}

esp_err_t um_mqtt_subscribe(const char *topic, int qos)
{
    if (!topic)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mqtt_state.enabled || !mqtt_state.connected || !mqtt_state.client)
    {
        ESP_LOGW(TAG, "Cannot subscribe: enabled=%d, connected=%d, client=%p",
                 mqtt_state.enabled, mqtt_state.connected, mqtt_state.client);
        return ESP_FAIL;
    }

    char full_topic[128];
    if (!um_mqtt_get_device_topic(topic, full_topic, sizeof(full_topic)))
    {
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_subscribe(mqtt_state.client, full_topic, qos);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to subscribe to %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Subscribed to %s", full_topic);
    return ESP_OK;
}

esp_err_t um_mqtt_subscribe_full(const char *full_topic, int qos)
{
    if (!full_topic)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mqtt_state.enabled || !mqtt_state.connected || !mqtt_state.client)
    {
        ESP_LOGW(TAG, "Cannot subscribe: enabled=%d, connected=%d, client=%p",
                 mqtt_state.enabled, mqtt_state.connected, mqtt_state.client);
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_subscribe(mqtt_state.client, full_topic, qos);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to subscribe to %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Subscribed to %s", full_topic);
    return ESP_OK;
}

esp_err_t um_mqtt_unsubscribe(const char *topic)
{
    if (!topic)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mqtt_state.enabled || !mqtt_state.connected || !mqtt_state.client)
    {
        return ESP_FAIL;
    }

    char full_topic[128];
    if (!um_mqtt_get_device_topic(topic, full_topic, sizeof(full_topic)))
    {
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_unsubscribe(mqtt_state.client, full_topic);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to unsubscribe from %s", full_topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Unsubscribed from %s", full_topic);
    return ESP_OK;
}

esp_err_t um_mqtt_register_device(const char *device_type)
{
    if (!mqtt_state.enabled || !mqtt_state.connected || !mqtt_state.client)
    {
        return ESP_FAIL;
    }

    char full_topic[128];
    um_mqtt_get_device_topic(UM_MQTT_TOPIC_REGISTER, full_topic, sizeof(full_topic));

    // Простая регистрация без JSON
    char reg_data[384];
    snprintf(reg_data, sizeof(reg_data),
             "{\"client_id\":\"%s\",\"type\":\"%s\",\"time\":%lld,\"heap\":%ld}",
             mqtt_state.client_id,
             device_type ? device_type : "unknown",
             esp_timer_get_time() / 1000000,
             esp_get_free_heap_size());

    int msg_id = esp_mqtt_client_publish(mqtt_state.client, full_topic,
                                         reg_data, 0, 1, 1);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to register device");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Device registered: %s", reg_data);
    log_free_heap(__FUNCTION__);
    return ESP_OK;
}

void um_mqtt_set_data_callback(um_mqtt_data_callback_t callback)
{
    mqtt_state.data_callback = callback;
    ESP_LOGI(TAG, "Data callback registered");
}

void um_mqtt_reconnect(void)
{
    if (!mqtt_state.client || !mqtt_state.initialized || !mqtt_state.enabled)
    {
        ESP_LOGW(TAG, "Cannot reconnect: client not initialized or disabled");
        return;
    }

    ESP_LOGI(TAG, "Forcing MQTT reconnection...");
    esp_mqtt_client_reconnect(mqtt_state.client);
}

bool um_mqtt_update_config(void)
{
    load_config_from_nvs();

    return mqtt_state.config_changed;
}

#endif // UM_FEATURE_ENABLED(MQTT)