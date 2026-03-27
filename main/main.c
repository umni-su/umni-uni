#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "base_config.h"
#include "um_events.h"
#include "um_storage.h"
#include "um_nvs.h"
#include "um_capabilities.h"

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

#if UM_FEATURE_ENABLED(SDCARD)
#include "um_sd.h"
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

static const char *TAG = "MAIN";

static bool has_connection = false;
static bool eth_connected = false;
static bool wifi_connected = false;

char *hostname = NULL;

TaskHandle_t um_sensors_task_handle = NULL;

void um_main_send_config(um_capability_t cap)
{
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
        config_str = NULL;
    }
}

static void publish_sensor_data(
    um_capability_t cap, // тип сенсора (NTC1, ONEWIRE, и т.д.)
    char *topic,         // MQTT топик
    char *serial,        // серийный номер (для 1-wire)
    double value,        // значение
    bool use_webhooks    // отправлять ли вебхуки
)
{
    if (!has_connection)
    {
        return;
    }
// MQTT отправка (один раз!)
#if UM_FEATURE_ENABLED(MQTT)
    if (um_mqtt_connected())
    {
        um_mqtt_sensor_payload_t payload = {
            .category = topic,
            .capability = cap,
            .serial = serial,
            .value = (float)value};
        um_mqtt_publish_sensor_payload(payload, 0, 0);
    }
#endif

// WEBHOOK отправка (один раз!)
#if UM_FEATURE_ENABLED(WEBHOOKS)
    if (use_webhooks)
    {
        char *string_data = NULL;
        asprintf(&string_data,
                 "{\"cap\":\"%s\", \"serial\":\"%s\", \"value\": \"%.2f\"}",
                 um_capabilities_get_name(cap),
                 serial ? serial : "null",
                 value);
        if (string_data)
        {
            um_webhooks_post_string_timeout(string_data, 2000);
            free(string_data);
        }
    }
#endif
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
#if UM_FEATURE_ENABLED(MQTT)
        um_mqtt_init(hostname != NULL ? hostname : "umni-unknown");
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)
        um_webserver_start();
#endif
    }
}
// Обработчик событий отключения от сети (Ethernet, WiFi)
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

// Чтение сенсоров, отправка конфигураций
void um_main_device_handler(void *args)
{
    um_mqtt_sensor_payload_t payload = {0};

    ESP_LOGI("SENSORS", "Start reading sensors");
    while (true)
    {
#if UM_FEATURE_ENABLED(MQTT)
        um_mqtt_register_device(UMNI_DEVICE);
#endif
#if UM_FEATURE_ENABLED(INPUTS) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_INPUTS);
#endif
#if UM_FEATURE_ENABLED(OUTPUTS) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_OUTPUTS);
#endif
#if UM_FEATURE_ENABLED(RF433) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_RF433);
#endif
#if UM_FEATURE_ENABLED(ONEWIRE)
#if UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_ONEWIRE);
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
                        publish_sensor_data(UM_CAP_ONEWIRE, UM_MQTT_TOPIC_ONEWIRE,
                                            sensor->serial, ow_temp, true);
                        ESP_LOGI(TAG, "[1-wire] sn:%s, temp: %.2f", sensor->serial, ow_temp);
                    }
                }
            }
        }
#endif
#if UM_FEATURE_ENABLED(NTC) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_NTC);
#endif
#if UM_FEATURE_ENABLED(NTC1)
        float temp1;
        if (um_ntc_read_temperature(UM_NTC_CHANNEL_1, &temp1) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_NTC1, UM_MQTT_TOPIC_NTC, NULL, temp1, false);
            ESP_LOGI(TAG, "[ntc] ntc1: %.2f", temp1);
        }
#endif
#if UM_FEATURE_ENABLED(NTC2)
        float temp2;
        if (um_ntc_read_temperature(UM_NTC_CHANNEL_2, &temp2) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_NTC2, UM_MQTT_TOPIC_NTC, NULL, temp2, false);
            ESP_LOGI(TAG, "[ntc] ntc2: %.2f", temp2);
        }
#endif

#if UM_FEATURE_ENABLED(AI) && UM_FEATURE_ENABLED(MQTT)
        um_main_send_config(UM_CAP_ADC);
#endif
#if UM_FEATURE_ENABLED(AI1)
        int adc1;
        if (um_adc_read_raw(UM_ADC_CHANNEL_1, &adc1) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_AI1, UM_MQTT_TOPIC_AI, NULL, adc1, false);
            ESP_LOGI(TAG, "[um_main_device_handler][adc] adc1, val: %d", adc1);
        }
#endif
#if UM_FEATURE_ENABLED(AI2)
        int adc2;
        if (um_adc_read_raw(UM_ADC_CHANNEL_2, &adc2) == ESP_OK)
        {
            publish_sensor_data(UM_CAP_AI2, UM_MQTT_TOPIC_AI, NULL, adc2, false);
            ESP_LOGI(TAG, "[um_main_device_handler][adc] adc2, val: %d", adc2);
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
    char *cpb = um_capabilities_get_json_object();

    ESP_LOGI(TAG, "Capabilities: %s", cpb);
    free(cpb);

    // Шина событий
    um_events_init();
    // NVS хранилище
    um_nvs_init();
    // Spiffs
    um_storage_init("/spiffs", NULL, 5, true);

    um_nvs_get_hostname(&hostname);

#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2) || UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
    esp_err_t ret_adc = um_adc_common_init();
    if (ret_adc != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC common");
    }
    else
    {
        ESP_LOGI(TAG, "ADC common handler initialize successfully");
        adc_oneshot_unit_handle_t *adc_handle = &um_adc_common_handle;
#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2)
        ESP_LOGI(TAG, "Initializing NTC system...");
        if (um_ntc_init(adc_handle) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize NTC");
            // Можно продолжить без NTC
        }
        um_ntc_set_all_enabled(true);
#endif

        // Инициализируем ADC если нужен
#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
        ESP_LOGI(TAG, "Initializing ADC system...");
        if (um_adc_init(adc_handle) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize ADC");
            // Можно продолжить без ADC
        }
        um_adc_set_all_enabled(true);
#endif
    }
#endif
    // ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ANY, um_main_event_handler, NULL));
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
            // Первый запуск - создаём дефолтный конфиг
            um_onewire_config_create_default();
            um_onewire_config_load();
        }

        um_onewire_config_apply();
    }
#endif

#if UM_FEATURE_ENABLED(ETHERNET)
    um_ethernet_init();
#endif

#if UM_FEATURE_ENABLED(SDCARD)
    um_sd_init();
#endif

#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC1)
    // Чтение всех температур
    float temp1, temp2;
    uint8_t success_read_ntc = um_ntc_read_all(&temp1, &temp2);

#if UM_FEATURE_ENABLED(NTC1)
    if (success_read_ntc & 0x01)
    {
        ESP_LOGI("MAIN", "NTC1: %.2f°C", temp1);
    }
#endif

#if UM_FEATURE_ENABLED(NTC2)
    if (success_read_ntc & 0x02)
    {
        ESP_LOGI("MAIN", "NTC2: %.2f°C", temp2);
    }
#endif

#endif

#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
    um_adc_set_all_enabled(true);
    int raw1, raw2;

    uint8_t success_read_adc = um_adc_read_all_raw(&raw1, &raw2);
#if UM_FEATURE_ENABLED(AI1)
    if (success_read_adc & 0x01)
    {
        ESP_LOGI("MAIN", "ADC1 raw: %d", raw1);
    }
#endif
#if UM_FEATURE_ENABLED(AI2)
    if (success_read_adc & 0x02)
    {
        ESP_LOGI("MAIN", "ADC2 raw: %d", raw2);
    }
#endif

#endif

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Приложение запущено успешно!");

    xTaskCreatePinnedToCore(um_main_device_handler, "um_main_device_handler", 4096, NULL, 3, &um_sensors_task_handle, 1);

#if UM_FEATURE_ENABLED(OPENTHERM)
    um_ot_init();
#endif
}