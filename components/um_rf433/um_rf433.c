#include "um_rf433.h"
#include "um_rf433_config.h"
#include "um_mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "rf433";

static um_rf_devices_t rf_scanned_devices[UM_RF433_MAX_SEARCH_SENSORS];

static QueueHandle_t esp_rf433_queue = NULL;

static bool search = false;

void um_rf433_receiver_task(void *pvParameter)
{
    uint8_t prot_num = 0;
    esp_rf433_queue = (QueueHandle_t)pvParameter;

    while (1)
    {
        if (esp_rf433_queue != NULL && xQueueReceive(esp_rf433_queue, &prot_num, portMAX_DELAY) == pdFALSE)
        {
            ESP_LOGE(TAG, "RF433 interrupt fail");
            continue;
        }

        uint32_t all = esp_rf433_get_received_value();
        int chan4 = all >> 0 & 0x01;
        int chan3 = all >> 1 & 0x01;
        int chan2 = all >> 2 & 0x01;
        int chan1 = all >> 3 & 0x01;

        uint8_t state = (chan1 << 0) | (chan2 << 1) | (chan3 << 2) | (chan4 << 3);
        uint32_t serial = all >> 4;

        // Поиск датчика в конфигурации
        int existing_index = um_rf433_config_get_index(serial);
        um_rf433_device_t *dev = (existing_index >= 0) ? &rf_devices[existing_index] : NULL;

        if (dev)
        {
            // Работаем с известным датчиком
            int64_t current_time = esp_timer_get_time();
            int64_t time_diff = (dev->time > 0) ? (current_time - dev->time) : 0;
            float time_diff_ms = time_diff / 1000.0f;

            // Обновляем время последнего приема
            dev->time = current_time;

            // Debouncing: считаем пакеты для стабилизации состояния
            dev->packet_count++;

            // Логика определения срабатывания (пример для датчика движения/двери)
            // triggered = true если пришло изменение состояния и прошло достаточно времени
            if (dev->state != state && time_diff_ms > 200)
            {
                dev->triggered = true;
                dev->last_processed_time = current_time;
                ESP_LOGI(TAG, "Device %06lX triggered: state changed %d -> %d",
                         serial, dev->state, state);
            }
            else
            {
                dev->triggered = false;
            }

            // Обновляем состояние
            dev->state = state;

            // Если это событие срабатывания - отправляем уведомление
            if (dev->triggered && dev->serial > 0)
            {
                ESP_LOGW(TAG, "=== TRIGGERED ===");
                ESP_LOGW(TAG, "Serial: %06lX, Alarm: %s",
                         dev->serial, dev->alarm ? "YES" : "no");
                ESP_LOGI(TAG, "State: %d, Channels: A:%d B:%d C:%d D:%d",
                         state, chan1, chan2, chan3, chan4);
                ESP_LOGI(TAG, "Time diff: %.1f ms, Packet #%d",
                         time_diff_ms, dev->packet_count);

                                // Отправка события в систему
                /*
                um_ev_message_rf433 message = {
                    .alarm = dev->alarm,
                    .serial = dev->serial,
                    .state = dev->state,
                    .triggered = dev->triggered,
                    .type = dev->type
                };
                esp_event_post(APP_EVENTS, EV_RF433_SENSOR, &message, sizeof(message), portMAX_DELAY);
                */
            }
            else
            {
                // Регулярные пакеты без срабатывания - логируем редко (каждый 10-й пакет)
                if (dev->packet_count % 10 == 0)
                {
                    ESP_LOGD(TAG, "Heartbeat from %06lX: state %d, packets: %d",
                             serial, state, dev->packet_count);
                }
            }
        }
        else
        {
            // Неизвестный датчик - только если не в режиме поиска
            if (!search)
            {
                ESP_LOGD(TAG, "Unknown device: Serial %06lX, State: %d, Channels: A:%d B:%d C:%d D:%d",
                         serial, state, chan1, chan2, chan3, chan4);
            }
        }

        // Режим поиска новых устройств (всегда работает, даже для известных)
        if (search)
        {
            um_rf_devices_t search_dev = {
                .serial = serial,
                .state = state,
                .time = esp_timer_get_time() // Добавим время для поиска
            };

            int search_array_length = um_rf433_get_array_length(rf_scanned_devices, UM_RF433_MAX_SEARCH_SENSORS);

            // Ищем в массиве поиска
            int existing_search_index = um_rf433_get_existing_index(rf_scanned_devices, search_dev.serial, UM_RF433_MAX_SEARCH_SENSORS);

            if (existing_search_index >= 0)
            {
                // Обновляем существующую запись
                rf_scanned_devices[existing_search_index] = search_dev;
            }
            else if (search_array_length < UM_RF433_MAX_SEARCH_SENSORS)
            {
                // Добавляем новую
                rf_scanned_devices[search_array_length] = search_dev;
                ESP_LOGI(TAG, "[SEARCH] New device found: %06lX (State: %d)", serial, state);
            }
            else
            {
                // Массив поиска полон - можно заменить самую старую запись
                // Для простоты пока ничего не делаем
                ESP_LOGW(TAG, "[SEARCH] Search array full, device %06lX not stored", serial);
            }
        }

        esp_rf433_reset_available();
    }

    vTaskDelete(NULL);
}

short int um_rf433_get_existing_index(um_rf_devices_t *devices, uint32_t number, int max)
{
    for (size_t i = 0; i < max; i++)
    {
        if (devices[i].serial == number)
        {
            return i;
        }
    }
    return -1;
}

short int um_rf433_get_array_length(um_rf_devices_t *devices, int max)
{
    short int count = 0;
    for (size_t i = 0; i < max; i++)
    {
        if (devices[i].serial > 0)
        {
            count++;
        }
    }
    return count;
}

void um_rf433_clear_search()
{
    for (int i = 0; i < UM_RF433_MAX_SEARCH_SENSORS; i++)
    {
        rf_scanned_devices[i].alarm = false;
        rf_scanned_devices[i].serial = 0;
        rf_scanned_devices[i].time = 0;
        rf_scanned_devices[i].triggered = false;
        rf_scanned_devices[i].state = 0;
    }
}

void um_rf_433_init()
{
    char *config = um_rf433_config_read();
    if (config == NULL)
    {
        um_rf433_config_create_empty();
        ESP_LOGI(TAG, "Default config created");
    }
    else
    {
        ESP_LOGI(TAG, "Load config: %s", config);
    }
    free(config);
    esp_rf433_initialize(CONFIG_UM_CFG_RF433_DATA_GPIO, &um_rf433_receiver_task);
}

void um_rf433_activale_search()
{
    search = true;
}