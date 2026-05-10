#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "base_config.h"
#include "um_events.h"
#include "um_storage.h"
#include "um_nvs.h"
#include "um_capabilities.h"
#include "um_helpers.h"
#include "um_sse_server.h"
#include "um_sockets.h"

#if UM_FEATURE_ENABLED(WEBHOOKS)
#include "um_webhooks.h"
#endif

#if UM_FEATURE_ENABLED(ETHERNET)
#include "um_ethernet.h"
#endif

#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
#include "um_opencollectors.h"
#endif

#if UM_FEATURE_ENABLED(BUZZER)
#include "um_buzzer.h"
#endif

#if UM_FEATURE_ENABLED(ALARM)
#include "um_alarm.h"
#endif

#if UM_FEATURE_ENABLED(INPUTS) || UM_FEATURE_ENABLED(OUTPUTS)
#include "um_dio.h"
#include "um_dio_config.h"
#endif

#if UM_FEATURE_ENABLED(OPENTHERM)
#include "um_opentherm.h"
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)
#include "um_webserver.h"
#endif

#if UM_FEATURE_ENABLED(ONEWIRE)
#include "um_onewire.h"
#include "um_onewire_config.h"
#endif

#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2) || UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
#include "um_adc_common.h"
#endif

#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2)
#include "um_ntc.h"
#include "um_ntc_config.h"
#endif

#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
#include "um_adc.h"
#include "um_adc_config.h"
#endif

#if UM_FEATURE_ENABLED(RF433)
#include "um_rf433.h"
#include "um_rf433_config.h"
#endif

#if UM_FEATURE_ENABLED(MQTT)
#include "um_mqtt.h"
#endif

static const char *TAG = "main";

static bool has_connection = false;
static bool eth_connected = false;
static bool wifi_connected = false;

char *hostname = NULL;

TaskHandle_t um_sensors_task_handle = NULL;

// ========== ОЧЕРЕДЬ ДЛЯ СЕНСОРОВ ==========
static QueueHandle_t sensor_queue = NULL;
static TaskHandle_t publisher_task_handle = NULL;

typedef struct
{
    um_capability_t cap;
    char category[32];
    char serial[32];
    double value;
    bool use_webhooks;
} sensor_queue_item_t;

// Задача-обработчик очереди
static void publisher_task(void *pvParameters)
{
    sensor_queue_item_t item;

    while (1)
    {
        if (xQueueReceive(sensor_queue, &item, portMAX_DELAY))
        {
            if (!has_connection)
                continue;

            cJSON *json_data = cJSON_CreateObject();
            if (!json_data)
                continue;

            const char *cap_str = um_capabilities_get_name(item.cap);

            cJSON_AddNumberToObject(json_data, "timestamp", um_helpers_get_real_timestamp_ms());
            cJSON_AddStringToObject(json_data, "capability", item.category);

            if (strlen(item.serial) == 0)
            {
                cJSON_AddNullToObject(json_data, "serial");
                cJSON_AddStringToObject(json_data, "identifier", cap_str);
            }
            else
            {
                cJSON_AddStringToObject(json_data, "serial", item.serial);
                cJSON_AddStringToObject(json_data, "identifier", item.serial);
            }
            cJSON_AddNumberToObject(json_data, "value", item.value);

            char *data = cJSON_PrintUnformatted(json_data);
            if (data)
            {
                um_sockets_send_syslog(item.category, data);
#if UM_FEATURE_ENABLED(WEBHOOKS)
                if (item.use_webhooks)
                {
                    um_webhooks_post_string_timeout(data, 2000);
                }
#endif
#if UM_FEATURE_ENABLED(WEBSERVER)
                um_sse_publish_event("sensor", data);
#endif
                free(data);
            }
            cJSON_Delete(json_data);
        }
    }
}

// Функция публикации (просто отправляет в очередь)
static void publish_sensor_data(um_capability_t cap, char *category,
                                const char *serial, double value, bool use_webhooks)
{
    if (!has_connection)
        return;
    if (sensor_queue == NULL)
        return;

    sensor_queue_item_t item;
    item.cap = cap;
    item.value = value;
    item.use_webhooks = use_webhooks;

    // Безопасное копирование строк
    strncpy(item.category, category, sizeof(item.category) - 1);
    item.category[sizeof(item.category) - 1] = '\0';

    if (serial && strlen(serial) > 0)
    {
        strncpy(item.serial, serial, sizeof(item.serial) - 1);
        item.serial[sizeof(item.serial) - 1] = '\0';
    }
    else
    {
        item.serial[0] = '\0';
    }

    // Отправляем в очередь (неблокирующая)
    if (xQueueSend(sensor_queue, &item, 0) != pdTRUE)
    {
        ESP_LOGW(TAG, "Sensor queue full, dropping data");
    }
}

// Обработчик событий (тоже отправляет в очередь)
void um_main_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id)
    {
    case UMNI_EVENT_SENSOR_CHANGED:
    {
        um_event_sensor_payload_t *payload = (um_event_sensor_payload_t *)data;
        if (payload && payload->category)
        {
            publish_sensor_data(
                (um_capability_t)payload->capability,
                payload->category,
                payload->serial,
                payload->value,
                false);
        }
        break;
    }
    default:
        break;
    }
}

// Обработчик события получения IP
void um_main_connected_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id)
    {
    case UMNI_EVENT_ETH_CONNECTED:
        eth_connected = true;
        has_connection = true;
        break;
    default:
        break;
    }
    if (has_connection)
    {
        um_helpers_time_init();
        um_helpers_mdns_init();

        uint16_t socket_port = 0;
        esp_err_t sock_res = um_nvs_get_socket_port(&socket_port);
        if (sock_res != ESP_OK)
        {
            socket_port = UM_NVS_DEFAULT_SOCKET_PORT;
        }
        um_sockets_init(socket_port);

#if UM_FEATURE_ENABLED(MQTT)
        um_mqtt_init(hostname != NULL ? hostname : "umni-unknown");
#endif
#if UM_FEATURE_ENABLED(WEBSERVER)
        um_webserver_start();
#endif
    }
}

// Обработчик событий отключения от сети
void um_main_disconnected_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id)
    {
    case UMNI_EVENT_ETH_DISCONNECTED:
        eth_connected = false;
        if (!wifi_connected)
        {
            has_connection = false;
        }
        break;
    default:
        break;
    }
    if (!has_connection)
    {
        um_sockets_deinit();
#if UM_FEATURE_ENABLED(MQTT)
        um_mqtt_deinit();
        ESP_LOGI(TAG, "Deinit MQTT");
#endif
#if UM_FEATURE_ENABLED(WEBSERVER)
        um_webserver_stop();
        ESP_LOGI(TAG, "Stop webserver");
#endif
    }
}

void um_main_send_config(um_capability_t cap)
{
#if UM_FEATURE_ENABLED(MQTT)
    char *config_str = NULL;
    const char *key = um_capabilities_get_name(cap);
    switch (cap)
    {
    case UM_CAP_ADC:
        config_str = um_adc_config_read();
        break;
    case UM_CAP_NTC:
        config_str = um_ntc_config_read();
        break;
    case UM_CAP_INPUTS:
        config_str = um_dio_config_get_inputs_json();
        break;
    case UM_CAP_OUTPUTS:
        config_str = um_dio_config_get_outputs_json();
        break;
    case UM_CAP_ONEWIRE:
        config_str = um_onewire_config_read();
        break;
    case UM_CAP_RF433:
        config_str = um_rf433_config_read();
        break;
    case UM_CAP_OPENTHERM:
        config_str = um_ot_get_status_json();
        break;
    default:
        break;
    }
    if (config_str != NULL)
    {
        cJSON *config_obj = cJSON_CreateObject();
        cJSON *config_json = cJSON_Parse(config_str);
        if (config_json != NULL)
        {
            cJSON_AddStringToObject(config_obj, "key", key);
            cJSON_AddItemToObject(config_obj, "config", config_json);
            char *publish_str = cJSON_PrintUnformatted(config_obj);
            if (publish_str != NULL)
                um_mqtt_publish(UM_MQTT_TOPIC_CONFIG, publish_str, 0, 0);
            free(publish_str);
        }
        cJSON_Delete(config_obj);
        free(config_str);
    }
#endif
}

// Чтение сенсоров
void um_main_device_handler(void *args)
{
    ESP_LOGI("SENSORS", "Start reading sensors");
    while (true)
    {
#if UM_FEATURE_ENABLED(MQTT)
        um_mqtt_register_device(UMNI_DEVICE);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
#endif
#if UM_FEATURE_ENABLED(INPUTS) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_INPUTS);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
#endif
#if UM_FEATURE_ENABLED(OUTPUTS) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_OUTPUTS);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
#endif
#if UM_FEATURE_ENABLED(RF433) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_RF433);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
#endif
#if UM_FEATURE_ENABLED(OPENTHERM) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_OPENTHERM);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
#endif

#if UM_FEATURE_ENABLED(ONEWIRE)
#if UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_ONEWIRE);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
#endif
        const um_onewire_state_t *onewire_state = um_onewire_get_state();
        if (onewire_state->initialized)
        {
            for (int i = 0; i < onewire_state->sensor_count; i++)
            {
                const um_onewire_sensor_t *sensor = &onewire_state->sensors[i];
                if (sensor->active)
                {
                    float ow_temp;
                    if (um_onewire_read_temperature(sensor->address, &ow_temp) == ESP_OK)
                    {
                        publish_sensor_data(UM_CAP_ONEWIRE, UM_CATEGORY_ONEWIRE,
                                            sensor->serial, ow_temp, true);
                    }
                }
            }
        }
#endif

#if UM_FEATURE_ENABLED(NTC1)
        float temp1;
        if (um_ntc_read_temperature(UM_NTC_CHANNEL_1, &temp1) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_NTC1, UM_CATEGORY_NTC, NULL, temp1, false);
        }
#endif

#if UM_FEATURE_ENABLED(NTC2)
        float temp2;
        if (um_ntc_read_temperature(UM_NTC_CHANNEL_2, &temp2) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_NTC2, UM_CATEGORY_NTC, NULL, temp2, false);
        }
#endif

#if UM_FEATURE_ENABLED(AI1)
        int adc1;
        if (um_adc_read_raw(UM_ADC_CHANNEL_1, &adc1) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_AI1, UM_CATEGORY_AI, NULL, adc1, false);
        }
#endif

#if UM_FEATURE_ENABLED(AI2)
        int adc2;
        if (um_adc_read_raw(UM_ADC_CHANNEL_2, &adc2) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_AI2, UM_CATEGORY_AI, NULL, adc2, false);
        }
#endif
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void shutdown_handler()
{
    ESP_LOGI(TAG, "Shutdown handler called. Performing cleanup...");
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Firmware Version: %s", CONFIG_UMNI_FW_VERSION);
    ESP_LOGI(TAG, "========================================");

    esp_register_shutdown_handler(shutdown_handler);

    um_capabilities_init();
    um_events_init();
    um_nvs_init();
    um_storage_init("/spiffs", NULL, 5, true);
    um_nvs_get_hostname(&hostname);

    // Создаем очередь для сенсоров
    sensor_queue = xQueueCreate(20, sizeof(sensor_queue_item_t));

    // Создаем отдельную задачу для обработки очереди (СТЕК 8192 вместо 4096)
    xTaskCreate(publisher_task, "sensor_pub", 8192, NULL, 2, &publisher_task_handle);

#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2) || UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
    esp_err_t ret_adc = um_adc_common_init();
    if (ret_adc != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC common");
    }
    else
    {
        adc_oneshot_unit_handle_t *adc_handle = &um_adc_common_handle;
#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2)
        if (um_ntc_init(adc_handle) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize NTC");
        }
        um_ntc_set_all_enabled(true);
        um_ntc_store_init();
#endif
#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
        if (um_adc_init(adc_handle) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize ADC");
        }
        um_adc_set_all_enabled(true);
#endif
    }
#endif

    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ANY, um_main_event_handler, NULL));
    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ETH_CONNECTED, um_main_connected_handler, NULL));
    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ETH_DISCONNECTED, um_main_disconnected_handler, NULL));

#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
    um_opencollectors_init();
#endif
#if UM_FEATURE_ENABLED(BUZZER)
    um_buzzer_init();
#endif
#if UM_FEATURE_ENABLED(ALARM)
    um_alarm_init(UM_ALARM_EDGE_BOTH, false, false, 400);
#endif
#if UM_FEATURE_ENABLED(INPUTS) || UM_FEATURE_ENABLED(OUTPUTS)
    um_dio_init();
#endif
#if UM_FEATURE_ENABLED(RF433)
    um_rf_433_init();
#endif
#if UM_FEATURE_ENABLED(ONEWIRE)
    esp_err_t ret = um_onewire_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize 1-Wire bus");
    }
    else
    {
        uint8_t sensor_count = um_onewire_scan();
        ESP_LOGI(TAG, "Found %d sensors", sensor_count);
        if (um_onewire_config_load() != ESP_OK)
        {
            um_onewire_config_create_default();
            um_onewire_config_load();
        }
        um_onewire_config_apply();
    }
#endif
#if UM_FEATURE_ENABLED(ETHERNET)
    vTaskDelay(pdMS_TO_TICKS(100));
    um_ethernet_init(NULL);
#endif

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Приложение запущено успешно!");

    xTaskCreatePinnedToCore(um_main_device_handler, "um_main_device_handler", 4096, NULL, 3, &um_sensors_task_handle, 1);

#if UM_FEATURE_ENABLED(OPENTHERM)
    um_ot_init();
#endif
}