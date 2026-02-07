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

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Приложение запущено успешно!");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}