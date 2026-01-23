#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "base_config.h"

#if UM_FEATURE_ENABLED(ETHERNET)
#include "um_ethernet.h"
#endif

static const char* TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Версия прошивки: %s", CONFIG_UMNI_FW_VERSION);
    ESP_LOGI(TAG, "========================================");
    
    ESP_LOGI(TAG, "Конфигурация:");
    ESP_LOGI(TAG, "  Ethernet: %s", CONFIG_UM_FEATURE_ETHERNET ? "ВКЛ" : "ВЫКЛ");
    ESP_LOGI(TAG, "  OpenTherm: %s (IN: %d)", 
             CONFIG_UM_FEATURE_OPENTHERM ? "ВКЛ" : "ВЫКЛ", CONFIG_UM_CFG_OT_IN_GPIO);
    ESP_LOGI(TAG, "  1-Wire: %s (PIN: %d)", 
             CONFIG_UM_FEATURE_ONEWIRE ? "ВКЛ" : "ВЫКЛ", CONFIG_UM_CFG_ONEWIRE_GPIO);
    
    // Пример использования в коде
    #if UM_FEATURE_ENABLED(OPENTHERM)
        ESP_LOGI(TAG, "OpenTherm доступен на пине %d", CONFIG_UM_CFG_OT_IN_GPIO);
    #endif

    #if UM_FEATURE_ENABLED(ONEWIRE)
        ESP_LOGI(TAG, "1-Wire доступен на пине %d", CONFIG_UM_CFG_ONEWIRE_GPIO);
    #endif
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Приложение запущено успешно!");

    #if UM_FEATURE_ENABLED(ETHERNET)
    hello();
    #endif
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}