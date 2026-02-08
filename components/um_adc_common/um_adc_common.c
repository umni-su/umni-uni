#include "um_adc_common.h"
#include "esp_log.h"
#include "base_config.h"

static const char *TAG = "adc_common";

// Экспортируем handle
adc_oneshot_unit_handle_t um_adc_common_handle = NULL;

static bool s_initialized = false;

esp_err_t um_adc_common_init(void)
{
    if (s_initialized)
    {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing ADC common...");
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &um_adc_common_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC common: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "ADC common initialized successfully");
    return ESP_OK;
}

adc_oneshot_unit_handle_t um_adc_common_get_handle(void)
{
    return um_adc_common_handle;
}

esp_err_t um_adc_common_deinit(void)
{
    if (!s_initialized)
    {
        return ESP_OK;
    }

    if (um_adc_common_handle != NULL)
    {
        adc_oneshot_del_unit(um_adc_common_handle);
        um_adc_common_handle = NULL;
        ESP_LOGI(TAG, "ADC common handle deleted");
    }

    s_initialized = false;
    ESP_LOGI(TAG, "ADC common deinitialized");

    return ESP_OK;
}

bool um_adc_common_is_initialized(void)
{
    return s_initialized;
}