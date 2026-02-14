#include "um_rf433.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "rf433";

static um_rf_devices_t rf_devices[UM_RF433_MAX_SENSORS];

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
            ESP_LOGE(TAG, "RF433 interrurpt fail");
        }
        else
        {
            uint32_t all = esp_rf433_get_received_value();
            int chan4 = all >> 0 & 0x01;
            int chan3 = all >> 1 & 0x01;
            int chan2 = all >> 2 & 0x01;
            int chan1 = all >> 3 & 0x01;

            // uint8_t state = (uint8_t)all;
            uint8_t state = 0x00;
            state |= (chan1 << 0) | (chan2 << 1) | (chan3 << 2) | (chan4 << 3);
            // uint32_t number = data && 0XFF;
            uint32_t number = all >> 4;

            int existing_index = um_rf433_get_existing_index(rf_devices, number, UM_RF433_MAX_SENSORS);

            bool show = true;

            um_rf_devices_t dev;

            float div = 0;

            if (existing_index > -1)
            {
                dev = rf_devices[existing_index];
                // Смотрим состояние датчика. Если срабатывания не было, то устанавливаем флаг срабатывания
                rf_devices[existing_index].time = esp_timer_get_time();
                div = (rf_devices[existing_index].time - dev.time) / 1000;

                dev.triggered = !dev.triggered && (div > 200);
                dev.state = state;
                ESP_LOGI(TAG, "%.1f : Existing serial number %06lX, time %ld", div, dev.serial, rf_devices[existing_index].time);
            }

            // show = (dev.triggered) || existing_index == -1;
            show = dev.triggered;
            if (show)
            {
                ESP_LOGW(TAG, "Received %lu / %dbit Protocol: %d", all, esp_rf433_get_received_bit_length(), prot_num);
                ESP_LOGI(TAG, "Serial number %06lX, time %ld, index %d", dev.serial, dev.time, existing_index);
                ESP_LOGI(TAG, "State: %d, ", state);
                ESP_LOGI(TAG, "A: %d, B: %d, C: %d, D: %d, \n", chan1, chan2, chan3, chan4);

                int ind = um_rf433_get_existing_index(rf_devices, number, UM_RF433_MAX_SENSORS);

                rf_devices[ind].state = state;

                // um_ev_message_rf433 message = {
                //     .alarm = rf_devices[ind].alarm,
                //     .serial = rf_devices[ind].serial,
                //     .state = rf_devices[ind].state,
                //     .triggered = rf_devices[ind].triggered};

                // esp_event_post(APP_EVENTS, EV_RF433_SENSOR, &message, sizeof(message), portMAX_DELAY);
            }
            else
            {
                // ESP_LOGI(TAG, "[Device not added] Serial: %06lX, State: %d", number, state);
                // ESP_LOGI(TAG, "[Device not added] Channels: A:%d B:%d C:%d D:%d", chan1, chan2, chan3, chan4);
            }

            if (search)
            {
                um_rf_devices_t search_dev = {
                    .serial = number,
                    .state = state};
                // search is mode active
                int search_array_length = um_rf433_get_array_length(rf_scanned_devices, UM_RF433_MAX_SEARCH_SENSORS);
                // int existing = um_rf_
                if (search_array_length < UM_RF433_MAX_SEARCH_SENSORS)
                {
                    int existing_search_index = um_rf433_get_existing_index(rf_scanned_devices, search_dev.serial, UM_RF433_MAX_SEARCH_SENSORS);
                    if (existing_search_index == -1)
                    {
                        rf_scanned_devices[search_array_length] = search_dev;
                    }
                    else
                    {
                        rf_scanned_devices[existing_search_index] = search_dev;
                    }
                }
            }
            else
            {
                um_rf433_clear_search();
            }

            esp_rf433_reset_available();
        }
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
    esp_rf433_initialize(CONFIG_UM_CFG_RF433_DATA_GPIO, &um_rf433_receiver_task);
}