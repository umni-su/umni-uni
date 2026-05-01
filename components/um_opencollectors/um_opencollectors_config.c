#include "um_opencollectors_config.h"
#include "um_opencollectors.h"
#include "um_storage.h"
#include "um_capabilities.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "um_oc_config";
static const char *OC_CONFIG_PATH = "/spiffs/opencollectors.json";

static um_oc_config_t oc_config = {
    .channel_count = 0};

// Поиск канала по номеру
static um_oc_config_item_t *find_channel(uint8_t channel)
{
    for (int i = 0; i < oc_config.channel_count; i++)
    {
        if (oc_config.channels[i].channel == channel)
        {
            return &oc_config.channels[i];
        }
    }
    return NULL;
}

// Обновить количество каналов из capabilities
static void update_counts_from_capabilities(void)
{
    oc_config.channel_count = 0;

    for (int ch = 0; ch < 2; ch++)
    {
        um_capability_t cap = (ch == 0) ? UM_CAP_OC1 : UM_CAP_OC2;
        if (um_capabilities_has(cap))
        {
            oc_config.channels[oc_config.channel_count].channel = ch;
            oc_config.channels[oc_config.channel_count].active = true;
            snprintf(oc_config.channels[oc_config.channel_count].label,
                     sizeof(oc_config.channels[0].label),
                     "OC%d", ch + 1);
            oc_config.channel_count++;
        }
    }
}

esp_err_t um_oc_config_load(void)
{
    update_counts_from_capabilities();

    if (!um_storage_file_exists(OC_CONFIG_PATH))
    {
        ESP_LOGW(TAG, "Config not found, creating default");
        return um_oc_config_create_default();
    }

    char *json_str = um_storage_read_json_string(OC_CONFIG_PATH);
    if (!json_str)
    {
        ESP_LOGE(TAG, "Failed to read config");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *channels_array = cJSON_GetObjectItem(root, "channels");
    if (cJSON_IsArray(channels_array))
    {
        cJSON *item;
        cJSON_ArrayForEach(item, channels_array)
        {
            cJSON *ch_json = cJSON_GetObjectItem(item, "channel");
            if (!cJSON_IsNumber(ch_json))
                continue;

            uint8_t channel = ch_json->valueint;
            um_oc_config_item_t *cfg = find_channel(channel);

            if (cfg)
            {
                cJSON *label = cJSON_GetObjectItem(item, "label");
                if (cJSON_IsString(label))
                {
                    strncpy(cfg->label, label->valuestring, sizeof(cfg->label) - 1);
                }

                cJSON *active = cJSON_GetObjectItem(item, "active");
                if (cJSON_IsBool(active))
                {
                    cfg->active = cJSON_IsTrue(active);
                }
            }
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "OC config loaded, %d channels", oc_config.channel_count);
    return ESP_OK;
}

esp_err_t um_oc_config_save(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *channels_array = cJSON_CreateArray();

    for (int i = 0; i < oc_config.channel_count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "channel", oc_config.channels[i].channel);
        cJSON_AddStringToObject(item, "label", oc_config.channels[i].label);
        cJSON_AddBoolToObject(item, "active", oc_config.channels[i].active);
        cJSON_AddItemToArray(channels_array, item);
    }

    cJSON_AddItemToObject(root, "channels", channels_array);
    cJSON_AddNumberToObject(root, "count", oc_config.channel_count);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str)
        return ESP_ERR_NO_MEM;

    esp_err_t ret = um_storage_write_json(OC_CONFIG_PATH, json_str);
    free(json_str);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "OC config saved");
    }

    return ret;
}

esp_err_t um_oc_config_create_default(void)
{
    update_counts_from_capabilities();
    return um_oc_config_save();
}

const um_oc_config_item_t *um_oc_config_get_channel(uint8_t channel)
{
    return find_channel(channel);
}

esp_err_t um_oc_config_update_channel(uint8_t channel, const char *label, bool active)
{
    um_oc_config_item_t *cfg = find_channel(channel);
    if (!cfg)
        return ESP_ERR_NOT_FOUND;

    if (label)
    {
        strncpy(cfg->label, label, sizeof(cfg->label) - 1);
    }
    cfg->active = active;

    ESP_LOGI(TAG, "Updated OC%d: '%s' active=%s", channel + 1, cfg->label, active ? "yes" : "no");
    return ESP_OK;
}

char *um_oc_config_get_json(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *channels_array = cJSON_CreateArray();

    for (int i = 0; i < oc_config.channel_count; i++)
    {
        um_oc_state_t state = UM_OC_STATE_OFF;
        um_opencollectors_get(oc_config.channels[i].channel, &state);

        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "index", oc_config.channels[i].channel);
        cJSON_AddStringToObject(item, "label", oc_config.channels[i].label);
        cJSON_AddBoolToObject(item, "active", oc_config.channels[i].active);
        cJSON_AddBoolToObject(item, "state", (state == UM_OC_STATE_ON));
        cJSON_AddItemToArray(channels_array, item);
    }

    cJSON_AddItemToObject(root, "opencollectors", channels_array);
    cJSON_AddNumberToObject(root, "count", oc_config.channel_count);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}