#include "um_ntc.h"
#include "esp_log.h"

static const char *TAG = "um_ntc";

/**
 * @brief NTC channel structure (internal)
 */
typedef struct
{
    um_ntc_state_t state;        /**< Current channel state */
    float temperature;           /**< Last read temperature in °C */
    ntc_device_handle_t ntc_dev; /**< NTC driver handle */
    adc_channel_t adc_channel;   /**< ADC channel number */
    bool enabled_by_feature;     /**< Enabled by feature flag */
} um_ntc_channel_internal_t;

// Static shared ADC handle
static adc_oneshot_unit_handle_t *s_adc_handle_ptr = NULL;
static bool s_system_initialized = false;

// Static channel instances
#if UM_FEATURE_ENABLED(NTC1)
static um_ntc_channel_internal_t s_channel1 = {
    .adc_channel = CONFIG_UM_CFG_NTC1_ADC_CHANNEL,
    .enabled_by_feature = true};
#endif

#if UM_FEATURE_ENABLED(NTC2)
static um_ntc_channel_internal_t s_channel2 = {
    .adc_channel = CONFIG_UM_CFG_NTC2_ADC_CHANNEL,
    .enabled_by_feature = true};
#endif

// Helper to get channel pointer
static um_ntc_channel_internal_t *get_channel_ptr(um_ntc_channel_id_t channel_id)
{
    switch (channel_id)
    {
    case UM_NTC_CHANNEL_1:
#if UM_FEATURE_ENABLED(NTC1)
        return &s_channel1;
#else
        return NULL;
#endif
    case UM_NTC_CHANNEL_2:
#if UM_FEATURE_ENABLED(NTC2)
        return &s_channel2;
#else
        return NULL;
#endif
    default:
        return NULL;
    }
}

// Initialize single channel
static esp_err_t init_channel(um_ntc_channel_internal_t *channel)
{
    if (!channel || !channel->enabled_by_feature)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Reset channel state
    channel->state = UM_NTC_STATE_DISABLED;
    channel->temperature = 0.0f;
    channel->ntc_dev = NULL;

    return ESP_OK;
}

// Enable/disable single channel
static esp_err_t set_channel_enable(um_ntc_channel_internal_t *channel, bool enable)
{
    if (!channel || !channel->enabled_by_feature)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (enable)
    {
        // Already enabled
        if (channel->state == UM_NTC_STATE_ENABLED)
        {
            return ESP_OK;
        }

        // Create NTC device configuration
        ntc_config_t ntc_config = {
            .b_value = 3950,
            .r25_ohm = 10000,
            .fixed_ohm = 10000,
            .vdd_mv = 3300,
            .circuit_mode = CIRCUIT_MODE_NTC_GND,
            .atten = ADC_ATTEN_DB_12,
            .channel = channel->adc_channel,
            .unit = ADC_UNIT_1};

        // Create NTC device with shared ADC handle
        esp_err_t ret = ntc_dev_create(&ntc_config, &channel->ntc_dev, s_adc_handle_ptr);
        if (ret != ESP_OK)
        {
            channel->state = UM_NTC_STATE_ERROR;
            ESP_LOGE(TAG, "Failed to create NTC device for channel %d: %s",
                     channel->adc_channel, esp_err_to_name(ret));
            return ret;
        }

        channel->state = UM_NTC_STATE_ENABLED;
        ESP_LOGI(TAG, "Channel %d enabled", channel->adc_channel);
    }
    else
    {
        // Already disabled
        if (channel->state == UM_NTC_STATE_DISABLED)
        {
            return ESP_OK;
        }

        // Delete NTC device if it exists
        if (channel->ntc_dev)
        {
            esp_err_t ret = ntc_dev_delete(channel->ntc_dev);
            if (ret != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to delete NTC device for channel %d: %s",
                         channel->adc_channel, esp_err_to_name(ret));
            }
            channel->ntc_dev = NULL;
        }

        channel->state = UM_NTC_STATE_DISABLED;
        ESP_LOGI(TAG, "Channel %d disabled", channel->adc_channel);
    }

    return ESP_OK;
}

// Public functions
esp_err_t um_ntc_init(adc_oneshot_unit_handle_t *adc_handle)
{
    if (s_system_initialized)
    {
        return ESP_OK;
    }

    if (!adc_handle || *adc_handle == NULL)
    {
        ESP_LOGE(TAG, "Invalid ADC handle provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Сохраняем указатель на общий ADC handle
    s_adc_handle_ptr = adc_handle;

    // Инициализируем каналы
#if UM_FEATURE_ENABLED(NTC1)
    s_channel1.state = UM_NTC_STATE_DISABLED;
    s_channel1.temperature = 0.0f;
    s_channel1.ntc_dev = NULL;
#endif

#if UM_FEATURE_ENABLED(NTC2)
    s_channel2.state = UM_NTC_STATE_DISABLED;
    s_channel2.temperature = 0.0f;
    s_channel2.ntc_dev = NULL;
#endif

    s_system_initialized = true;
    ESP_LOGI(TAG, "NTC system initialized with shared ADC handle");
    return ESP_OK;
}

esp_err_t um_ntc_read_temperature(um_ntc_channel_id_t channel_id, float *temperature)
{
    if (!temperature)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_ntc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        ESP_LOGE(TAG, "Channel %d not available", channel_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Check if channel is enabled
    if (channel->state != UM_NTC_STATE_ENABLED)
    {
        ESP_LOGW(TAG, "Channel %d not enabled, state: %d", channel_id, channel->state);
        return ESP_ERR_INVALID_STATE;
    }

    // Read temperature
    esp_err_t ret = ntc_dev_get_temperature(channel->ntc_dev, temperature);
    if (ret == ESP_OK)
    {
        channel->temperature = *temperature;
        ESP_LOGD(TAG, "Channel %d temperature: %.2f°C", channel_id, *temperature);
    }
    else
    {
        channel->state = UM_NTC_STATE_ERROR;
        ESP_LOGE(TAG, "Failed to read temperature from channel %d: %s",
                 channel_id, esp_err_to_name(ret));
    }

    return ret;
}

um_ntc_state_t um_ntc_get_state(um_ntc_channel_id_t channel_id)
{
    um_ntc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        return UM_NTC_STATE_DISABLED;
    }
    return channel->state;
}

esp_err_t um_ntc_set_channel_enabled(um_ntc_channel_id_t channel_id, bool enable)
{
    if (!s_system_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    um_ntc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        ESP_LOGE(TAG, "Channel %d not available", channel_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return set_channel_enable(channel, enable);
}

esp_err_t um_ntc_get_last_temperature(um_ntc_channel_id_t channel_id, float *temperature)
{
    if (!temperature)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_ntc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (channel->state != UM_NTC_STATE_ENABLED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    *temperature = channel->temperature;
    return ESP_OK;
}

esp_err_t um_ntc_set_all_enabled(bool enable)
{
    if (!s_system_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    esp_err_t single_ret;

#if UM_FEATURE_ENABLED(NTC1)
    single_ret = set_channel_enable(&s_channel1, enable);
    if (single_ret != ESP_OK && ret == ESP_OK)
    {
        ret = single_ret;
    }
#endif

#if UM_FEATURE_ENABLED(NTC2)
    single_ret = set_channel_enable(&s_channel2, enable);
    if (single_ret != ESP_OK && ret == ESP_OK)
    {
        ret = single_ret;
    }
#endif

    return ret;
}

uint8_t um_ntc_read_all(float *temp1, float *temp2)
{
    if (!s_system_initialized)
    {
        return 0;
    }

    uint8_t success_mask = 0;
    esp_err_t ret;

#if UM_FEATURE_ENABLED(NTC1)
    if (temp1)
    {
        ret = um_ntc_read_temperature(UM_NTC_CHANNEL_1, temp1);
        if (ret == ESP_OK)
        {
            success_mask |= 0x01;
        }
    }
#endif

#if UM_FEATURE_ENABLED(NTC2)
    if (temp2)
    {
        ret = um_ntc_read_temperature(UM_NTC_CHANNEL_2, temp2);
        if (ret == ESP_OK)
        {
            success_mask |= 0x02;
        }
    }
#endif

    return success_mask;
}