```c
#include "um_adc.h"

void app_main(void)
{
    // Инициализация
    esp_err_t ret = um_adc_init();
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to initialize ADC");
        return;
    }

    // Включаем все каналы
    um_adc_set_all_enabled(true);

    // Чтение значений
    int raw1, raw2;
    
    // Чтение всех каналов
    uint8_t success = um_adc_read_all_raw(&raw1, &raw2);
    
    if (success & 0x01) {
        ESP_LOGI("MAIN", "ADC1 raw: %d", raw1);
    }
    
    if (success & 0x02) {
        ESP_LOGI("MAIN", "ADC2 raw: %d", raw2);
    }

    // Или чтение отдельного канала
    int raw_value;
    if (um_adc_read_raw(UM_ADC_CHANNEL_1, &raw_value) == ESP_OK) {
        ESP_LOGI("MAIN", "Channel 1: %d", raw_value);
    }

    // Отключение и освобождение ресурсов
    um_adc_deinit();
}
```