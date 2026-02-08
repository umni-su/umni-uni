#include "um_adc.h"
#include "esp_log.h"

static const char *TAG = "um_adc";

/**
 * @brief ADC channel structure (internal)
 */
typedef struct
{
    um_adc_state_t state;      /**< Current channel state */
    int last_raw_value;        /**< Last read raw ADC value */
    adc_channel_t adc_channel; /**< ADC channel number */
    adc_unit_t adc_unit;       /**< ADC unit */
} um_adc_channel_internal_t;

// Static ADC handle pointer
static adc_oneshot_unit_handle_t *s_adc_handle_ptr = NULL;
static bool s_system_initialized = false;

// Static channel instances
#if UM_FEATURE_ENABLED(AI1)
static um_adc_channel_internal_t s_channel1 = {
    .state = UM_ADC_STATE_DISABLED,
    .last_raw_value = 0,
    .adc_channel = CONFIG_UM_CFG_AI1_ADC_CHANNEL,
    .adc_unit = ADC_UNIT_1};
#endif

#if UM_FEATURE_ENABLED(AI2)
static um_adc_channel_internal_t s_channel2 = {
    .state = UM_ADC_STATE_DISABLED,
    .last_raw_value = 0,
    .adc_channel = CONFIG_UM_CFG_AI2_ADC_CHANNEL,
    .adc_unit = ADC_UNIT_1 // Assuming both use ADC1
};
#endif

// Helper to get channel pointer
static um_adc_channel_internal_t *get_channel_ptr(um_adc_channel_id_t channel_id)
{
    switch (channel_id)
    {
    case UM_ADC_CHANNEL_1:
#if UM_FEATURE_ENABLED(AI1)
        return &s_channel1;
#else
        return NULL;
#endif
    case UM_ADC_CHANNEL_2:
#if UM_FEATURE_ENABLED(AI2)
        return &s_channel2;
#else
        return NULL;
#endif
    default:
        return NULL;
    }
}

// Configure channel
static esp_err_t configure_channel(um_adc_channel_internal_t *channel)
{
    if (!channel)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_adc_handle_ptr || *s_adc_handle_ptr == NULL)
    {
        ESP_LOGE(TAG, "ADC handle not initialized for channel %d",
                 channel->adc_channel);
        return ESP_ERR_INVALID_STATE;
    }

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t ret = adc_oneshot_config_channel(*s_adc_handle_ptr, channel->adc_channel, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s",
                 channel->adc_channel, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "ADC channel %d configured", channel->adc_channel);
    return ESP_OK;
}

// Enable/disable single channel
static esp_err_t set_channel_enable(um_adc_channel_internal_t *channel, bool enable)
{
    if (!channel)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (enable)
    {
        // Already enabled
        if (channel->state == UM_ADC_STATE_ENABLED)
        {
            return ESP_OK;
        }

        // Check if ADC handle is valid
        if (!s_adc_handle_ptr || *s_adc_handle_ptr == NULL)
        {
            ESP_LOGE(TAG, "ADC handle not initialized");
            channel->state = UM_ADC_STATE_ERROR;
            return ESP_ERR_INVALID_STATE;
        }

        // Configure channel
        esp_err_t ret = configure_channel(channel);
        if (ret != ESP_OK)
        {
            channel->state = UM_ADC_STATE_ERROR;
            return ret;
        }

        channel->state = UM_ADC_STATE_ENABLED;
        ESP_LOGI(TAG, "ADC channel %d enabled", channel->adc_channel);
    }
    else
    {
        // Already disabled
        if (channel->state == UM_ADC_STATE_DISABLED)
        {
            return ESP_OK;
        }

        // Note: We don't unconfigure the channel since oneshot API doesn't have unconfig
        channel->state = UM_ADC_STATE_DISABLED;
        ESP_LOGI(TAG, "ADC channel %d disabled", channel->adc_channel);
    }

    return ESP_OK;
}

// Read raw ADC value from channel
static esp_err_t read_channel_raw(um_adc_channel_internal_t *channel, int *raw_value)
{
    if (!channel || !raw_value)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if channel is enabled
    if (channel->state != UM_ADC_STATE_ENABLED)
    {
        ESP_LOGW(TAG, "ADC channel %d not enabled, state: %d",
                 channel->adc_channel, channel->state);
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_adc_handle_ptr || *s_adc_handle_ptr == NULL)
    {
        ESP_LOGE(TAG, "ADC handle not initialized");
        channel->state = UM_ADC_STATE_ERROR;
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = adc_oneshot_read(*s_adc_handle_ptr, channel->adc_channel, raw_value);
    if (ret != ESP_OK)
    {
        channel->state = UM_ADC_STATE_ERROR;
        ESP_LOGE(TAG, "Failed to read ADC channel %d: %s",
                 channel->adc_channel, esp_err_to_name(ret));
        return ret;
    }

    channel->last_raw_value = *raw_value;
    ESP_LOGD(TAG, "ADC channel %d: raw=%d", channel->adc_channel, *raw_value);

    return ESP_OK;
}

// Public functions
esp_err_t um_adc_init(adc_oneshot_unit_handle_t *adc_handle)
{
    if (s_system_initialized)
    {
        return ESP_OK;
    }

    if (!adc_handle)
    {
        ESP_LOGE(TAG, "Invalid ADC handle pointer provided");
        return ESP_ERR_INVALID_ARG;
    }

    // Сохраняем указатель на общий ADC handle
    s_adc_handle_ptr = adc_handle;

    // Initialize channels state
#if UM_FEATURE_ENABLED(AI1)
    s_channel1.state = UM_ADC_STATE_DISABLED;
    s_channel1.last_raw_value = 0;
#endif

#if UM_FEATURE_ENABLED(AI2)
    s_channel2.state = UM_ADC_STATE_DISABLED;
    s_channel2.last_raw_value = 0;
#endif

    s_system_initialized = true;
    ESP_LOGI(TAG, "ADC system initialized with shared ADC handle");
    return ESP_OK;
}

esp_err_t um_adc_read_raw(um_adc_channel_id_t channel_id, int *raw_value)
{
    if (!raw_value)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_adc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        ESP_LOGE(TAG, "ADC channel %d not available", channel_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return read_channel_raw(channel, raw_value);
}

um_adc_state_t um_adc_get_state(um_adc_channel_id_t channel_id)
{
    um_adc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        return UM_ADC_STATE_DISABLED;
    }
    return channel->state;
}

esp_err_t um_adc_set_channel_enabled(um_adc_channel_id_t channel_id, bool enable)
{
    if (!s_system_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    um_adc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        ESP_LOGE(TAG, "ADC channel %d not available", channel_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return set_channel_enable(channel, enable);
}

esp_err_t um_adc_get_last_raw(um_adc_channel_id_t channel_id, int *raw_value)
{
    if (!raw_value)
    {
        return ESP_ERR_INVALID_ARG;
    }

    um_adc_channel_internal_t *channel = get_channel_ptr(channel_id);
    if (!channel)
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (channel->state != UM_ADC_STATE_ENABLED)
    {
        return ESP_ERR_INVALID_STATE;
    }

    *raw_value = channel->last_raw_value;
    return ESP_OK;
}

esp_err_t um_adc_set_all_enabled(bool enable)
{
    if (!s_system_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = ESP_OK;
    esp_err_t single_ret;

#if UM_FEATURE_ENABLED(AI1)
    single_ret = set_channel_enable(&s_channel1, enable);
    if (single_ret != ESP_OK && ret == ESP_OK)
    {
        ret = single_ret;
    }
#endif

#if UM_FEATURE_ENABLED(AI2)
    single_ret = set_channel_enable(&s_channel2, enable);
    if (single_ret != ESP_OK && ret == ESP_OK)
    {
        ret = single_ret;
    }
#endif

    return ret;
}

uint8_t um_adc_read_all_raw(int *raw1, int *raw2)
{
    if (!s_system_initialized)
    {
        return 0;
    }

    uint8_t success_mask = 0;
    esp_err_t ret;

#if UM_FEATURE_ENABLED(AI1)
    if (raw1)
    {
        ret = um_adc_read_raw(UM_ADC_CHANNEL_1, raw1);
        if (ret == ESP_OK)
        {
            success_mask |= 0x01;
        }
    }
#endif

#if UM_FEATURE_ENABLED(AI2)
    if (raw2)
    {
        ret = um_adc_read_raw(UM_ADC_CHANNEL_2, raw2);
        if (ret == ESP_OK)
        {
            success_mask |= 0x02;
        }
    }
#endif

    return success_mask;
}

esp_err_t um_adc_deinit(void)
{
    if (!s_system_initialized)
    {
        return ESP_OK;
    }

    // Disable all channels
    um_adc_set_all_enabled(false);

    // Note: We don't delete the ADC unit here because it's managed by um_adc_common
    // The handle pointer is owned by um_adc_common, so we just reset our pointer
    s_adc_handle_ptr = NULL;

    s_system_initialized = false;
    ESP_LOGI(TAG, "ADC system deinitialized");

    return ESP_OK;
}