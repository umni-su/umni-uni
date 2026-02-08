#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "base_config.h"
#include "um_events.h"
#include "um_storage.h"
#include "um_nvs.h"

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
#endif

#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
#include "um_adc.h"
#endif

#if UM_FEATURE_ENABLED(RF433)
#include "um_rf433.h"
#endif

static const char *TAG = "MAIN";

// Обработчик события 1
void handler1(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    ESP_LOGI(TAG, "Handler1: Получено событие %ld", (long)id);
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Версия прошивки: %s", CONFIG_UMNI_FW_VERSION);
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Конфигурация:");
    ESP_LOGI(TAG, "  Ethernet: %s", CONFIG_UM_FEATURE_ETHERNET ? "ВКЛ" : "ВЫКЛ");

    // Шина событий
    um_events_init();
    // NVS хранилище
    um_nvs_init();
    // Spiffs
    um_storage_init("/spiffs", NULL, 5, true);

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

    ESP_ERROR_CHECK(um_event_subscribe(UMNI_EVENT_ANY, handler1, NULL));

// Пример использования в коде
#if UM_FEATURE_ENABLED(OPENTHERM)
    um_ot_init();
    ESP_LOGI(TAG, "OpenTherm доступен на пине %d", CONFIG_UM_CFG_OT_IN_GPIO);
#endif

#if UM_FEATURE_ENABLED(ONEWIRE)
    ESP_LOGI(TAG, "1-Wire доступен на пине %d", CONFIG_UM_CFG_ONEWIRE_GPIO);
#endif

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

#if UM_FEATURE_ENABLED(WEBSERVER)
    um_webserver_start();
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

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}