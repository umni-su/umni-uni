#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "../include/features/features.h"


static const char* TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "UMNI Устройство: %s", UMNI_DEVICE_NAME);
    ESP_LOGI(TAG, "Модель: %s", UMNI_DEVICE_MODEL);
    ESP_LOGI(TAG, "Версия прошивки: %s", UMNI_FW_VERSION);
    ESP_LOGI(TAG, "========================================");
    
    ESP_LOGI(TAG, "Конфигурация:");
    ESP_LOGI(TAG, "  Ethernet: %s", UM_FEATURE_ETH ? "ВКЛ" : "ВЫКЛ");
    ESP_LOGI(TAG, "  OpenTherm: %s (IN: %d)", 
             UM_FEATURE_OT ? "ВКЛ" : "ВЫКЛ", UM_CFG_OT_IN);
    ESP_LOGI(TAG, "  1-Wire: %s (PIN: %d)", 
             UM_FEATURE_OW ? "ВКЛ" : "ВЫКЛ", UM_CFG_ONEWIRE_PIN);
    
    // Пример использования в коде
    IF_FEATURE_ENABLED(OT, {
        ESP_LOGI(TAG, "OpenTherm доступен на пине %d", UM_CFG_OT_IN);
    });

    IF_FEATURE_ENABLED(OW, {
        ESP_LOGI(TAG, "1-Wire доступен на пине %d", UM_CFG_ONEWIRE_PIN);
    });
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Приложение запущено успешно!");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}