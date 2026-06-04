#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "um_ntc_config.h"
#include "um_storage.h"
#include "base_config.h"

static const char *TAG = "um_ntc_config";

// Storage for channel configurations
static um_ntc_channel_config_t s_channel_configs[NTC_MAX_CHANNELS];
static uint8_t s_config_count = 0;
static const char *s_config_path = "/spiffs/ntc.json";

// Helper to check if channel is available by feature flag
static bool is_channel_available(uint8_t channel_id)
{
    switch (channel_id)
    {
    case 0: // UM_NTC_CHANNEL_1
#if UM_FEATURE_ENABLED(NTC1)
        return true;
#else
        return false;
#endif
    case 1: // UM_NTC_CHANNEL_2
#if UM_FEATURE_ENABLED(NTC2)
        return true;
#else
        return false;
#endif
    default:
        return false;
    }
}

// Helper to find configuration by channel ID
static um_ntc_channel_config_t *find_config_by_channel(uint8_t channel_id)
{
    for (int i = 0; i < s_config_count; i++)
    {
        if (s_channel_configs[i].channel_id == channel_id)
        {
            return &s_channel_configs[i];
        }
    }
    return NULL;
}

esp_err_t um_ntc_config_load(void)
{
    // Check if config file exists
    if (!um_storage_file_exists(s_config_path))
    {
        ESP_LOGW(TAG, "Config file %s not found, creating default", s_config_path);
        return um_ntc_config_create_default();
    }

    // Read JSON config
    char *config_json = um_storage_read_json_string(s_config_path);
    if (config_json == NULL)
    {
        ESP_LOGE(TAG, "Failed to read config file");
        return ESP_FAIL;
    }

    // Parse JSON
    cJSON *root = cJSON_Parse(config_json);
    free(config_json);

    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON config");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Clear old configurations
    s_config_count = 0;
    memset(s_channel_configs, 0, sizeof(s_channel_configs));

    // Read channels array
    cJSON *channels_array = cJSON_GetObjectItem(root, "channels");
    if (cJSON_IsArray(channels_array))
    {
        cJSON *channel_item = NULL;
        cJSON_ArrayForEach(channel_item, channels_array)
        {
            if (s_config_count >= NTC_MAX_CHANNELS)
            {
                ESP_LOGW(TAG, "Too many channels in config, max is %d", NTC_MAX_CHANNELS);
                break;
            }

            cJSON *id = cJSON_GetObjectItem(channel_item, "id");
            cJSON *label = cJSON_GetObjectItem(channel_item, "label");

            if (cJSON_IsNumber(id) && cJSON_IsString(label))
            {
                uint8_t channel_id = (uint8_t)id->valueint;

                // Only load config for available channels
                if (!is_channel_available(channel_id))
                {
                    ESP_LOGI(TAG, "Skipping config for unavailable channel %d", channel_id);
                    continue;
                }

                um_ntc_channel_config_t *config = &s_channel_configs[s_config_count];
                config->channel_id = channel_id;
                strncpy(config->label, label->valuestring, sizeof(config->label) - 1);

                // Read optional fields
                cJSON *active = cJSON_GetObjectItem(channel_item, "active");
                config->active = (active && cJSON_IsBool(active)) ? cJSON_IsTrue(active) : true;

                cJSON *calibration = cJSON_GetObjectItem(channel_item, "offset");
                config->calibration = (calibration && cJSON_IsNumber(calibration)) ? calibration->valuedouble : 0.0f;

                ESP_LOGI(TAG, "Loaded config for NTC channel %d: '%s' (active: %s, cal: %.2f)",
                         channel_id, config->label, config->active ? "yes" : "no",
                         config->calibration);

                s_config_count++;
            }
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d NTC channel configurations", s_config_count);
    return ESP_OK;
}

esp_err_t um_ntc_config_save(void)
{
    // Create JSON object
    cJSON *root = cJSON_CreateObject();
    cJSON *channels_array = cJSON_CreateArray();

    // Add all channel configurations
    for (int i = 0; i < s_config_count; i++)
    {
        const um_ntc_channel_config_t *config = &s_channel_configs[i];

        cJSON *channel = cJSON_CreateObject();
        cJSON_AddNumberToObject(channel, "id", config->channel_id);
        cJSON_AddStringToObject(channel, "label", config->label);
        cJSON_AddBoolToObject(channel, "active", config->active);

        double rounded = round(config->calibration * 10.0) / 10.0;
        cJSON_AddNumberToObject(channel, "offset", rounded);

        cJSON_AddItemToArray(channels_array, channel);
    }

    cJSON_AddItemToObject(root, "channels", channels_array);

    // Convert to string
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL)
    {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    // Save to file
    esp_err_t ret = um_storage_write_json(s_config_path, json_str);

    free(json_str);
    cJSON_Delete(root);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Saved %d NTC channel configurations to %s",
                 s_config_count, s_config_path);
    }

    return ret;
}

void um_ntc_config_apply(void)
{
    // Apply configuration to each available channel
    for (int i = 0; i < s_config_count; i++)
    {
        const um_ntc_channel_config_t *config = &s_channel_configs[i];

        um_ntc_channel_id_t channel_id = (config->channel_id == 0) ? UM_NTC_CHANNEL_1 : UM_NTC_CHANNEL_2;

        // Enable/disable based on active flag
        esp_err_t ret = um_ntc_set_channel_enabled(channel_id, config->active);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to %s NTC channel %d: %s",
                     config->active ? "enable" : "disable",
                     config->channel_id, esp_err_to_name(ret));
        }
        else
        {
            ESP_LOGI(TAG, "Applied config to NTC channel %d: active=%s",
                     config->channel_id, config->active ? "yes" : "no");
        }
    }
}

char *um_ntc_config_read(void)
{
    return um_storage_read_json_string(s_config_path);
}

esp_err_t um_ntc_config_update(uint8_t channel_id, const um_ntc_channel_config_t *config)
{
    if (config == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if channel is available
    if (!is_channel_available(channel_id))
    {
        ESP_LOGE(TAG, "Channel %d is not available", channel_id);
        return ESP_ERR_NOT_SUPPORTED;
    }

    um_ntc_channel_config_t *existing_config = find_config_by_channel(channel_id);
    if (existing_config)
    {
        // Update existing configuration
        *existing_config = *config;
        existing_config->channel_id = channel_id; // Ensure ID is correct
        existing_config->calibration = config->calibration;
        strncpy(existing_config->label, config->label, sizeof(existing_config->label) - 1);
        ESP_LOGI(TAG, "Updated config for NTC channel %d, act: %d, cal: %0.1f", channel_id, existing_config->active, existing_config->calibration);
    }
    else
    {
        // Add new configuration
        if (s_config_count >= NTC_MAX_CHANNELS)
        {
            return ESP_ERR_NO_MEM;
        }

        s_channel_configs[s_config_count] = *config;
        s_channel_configs[s_config_count].channel_id = channel_id;
        s_config_count++;
        ESP_LOGI(TAG, "Added new config for NTC channel %d", channel_id);
    }

    return ESP_OK;
}

const um_ntc_channel_config_t *um_ntc_config_get(uint8_t channel_id)
{
    return find_config_by_channel(channel_id);
}

esp_err_t um_ntc_config_create_default(void)
{
    // Reset configurations
    s_config_count = 0;
    memset(s_channel_configs, 0, sizeof(s_channel_configs));

    // Create default config for available channels
#if UM_FEATURE_ENABLED(NTC1)
    um_ntc_channel_config_t config1 = {
        .channel_id = 0,
        .active = true,
        .calibration = 0.0f};
    snprintf(config1.label, sizeof(config1.label), "NTC Sensor 1");
    s_channel_configs[s_config_count++] = config1;
    ESP_LOGI(TAG, "Created default config for NTC channel 1");
#endif

#if UM_FEATURE_ENABLED(NTC2)
    um_ntc_channel_config_t config2 = {
        .channel_id = 1,
        .active = true,
        .calibration = 0.0f};
    snprintf(config2.label, sizeof(config2.label), "NTC Sensor 2");
    s_channel_configs[s_config_count++] = config2;
    ESP_LOGI(TAG, "Created default config for NTC channel 2");
#endif

    // Apply and save
    um_ntc_config_apply();
    return um_ntc_config_save();
}