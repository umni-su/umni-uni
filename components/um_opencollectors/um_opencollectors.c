/**
 * @file um_opencollectors.c
 * @brief Simple open-collector outputs implementation
 */
#include "base_config.h"
#include "um_opencollectors.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "um_nvs.h"

#if UM_FEATURE_ENABLED(OPENCOLLECTORS)

static const char* TAG = "um_oc";

// Channel context
typedef struct {
    bool enabled;
    int gpio_num;
    um_oc_state_t state;
} oc_channel_t;

static oc_channel_t channels[2] = {0};

// Convert state to GPIO level (ON = LOW, OFF = HIGH for NPN transistor)
static inline int state_to_level(um_oc_state_t state) {
    return (state == UM_OC_STATE_ON) ? 0 : 1;
}

esp_err_t um_opencollectors_init(void) {
    esp_err_t ret = ESP_OK;
    int saved_states = -1;
    
    // 1. Загружаем состояния из NVS (один раз для всех каналов)
#if UM_FEATURE_ENABLED(OC1) || UM_FEATURE_ENABLED(OC2)
    saved_states = um_nvs_read_i8(UM_NVS_KEY_OPENCOLLECTORS);
#endif
    
    // 2. Инициализируем OC1
#if UM_FEATURE_ENABLED(OC1)
    channels[UM_OC_CHANNEL_1].gpio_num = CONFIG_UM_CFG_OC1_GPIO;
    if (channels[UM_OC_CHANNEL_1].gpio_num >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << channels[UM_OC_CHANNEL_1].gpio_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        
        if (gpio_config(&io_conf) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure OC1 GPIO %d", 
                     channels[UM_OC_CHANNEL_1].gpio_num);
            ret = ESP_FAIL;
        } else {
            channels[UM_OC_CHANNEL_1].enabled = true;
            
            // Устанавливаем состояние из NVS или OFF по умолчанию
            if (saved_states >= 0) {
                channels[UM_OC_CHANNEL_1].state = (saved_states & OC1_STATE_MASK) ? 
                                                  UM_OC_STATE_ON : UM_OC_STATE_OFF;
            } else {
                channels[UM_OC_CHANNEL_1].state = UM_OC_STATE_OFF;
            }
            
            gpio_set_level(channels[UM_OC_CHANNEL_1].gpio_num, 
                           state_to_level(channels[UM_OC_CHANNEL_1].state));
            
            ESP_LOGI(TAG, "OC1 initialized on GPIO %d (%s)", 
                     channels[UM_OC_CHANNEL_1].gpio_num,
                     channels[UM_OC_CHANNEL_1].state == UM_OC_STATE_ON ? "ON" : "OFF");
        }
    }
#endif
    
    // 3. Инициализируем OC2
#if UM_FEATURE_ENABLED(OC2)
    channels[UM_OC_CHANNEL_2].gpio_num = CONFIG_UM_CFG_OC2_GPIO;
    if (channels[UM_OC_CHANNEL_2].gpio_num >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << channels[UM_OC_CHANNEL_2].gpio_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        
        if (gpio_config(&io_conf) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure OC2 GPIO %d", 
                     channels[UM_OC_CHANNEL_2].gpio_num);
            ret = ESP_FAIL;
        } else {
            channels[UM_OC_CHANNEL_2].enabled = true;
            
            // Устанавливаем состояние из NVS или OFF по умолчанию
            if (saved_states >= 0) {
                channels[UM_OC_CHANNEL_2].state = (saved_states & OC2_STATE_MASK) ? 
                                                  UM_OC_STATE_ON : UM_OC_STATE_OFF;
            } else {
                channels[UM_OC_CHANNEL_2].state = UM_OC_STATE_OFF;
            }
            
            gpio_set_level(channels[UM_OC_CHANNEL_2].gpio_num, 
                           state_to_level(channels[UM_OC_CHANNEL_2].state));
            
            ESP_LOGI(TAG, "OC2 initialized on GPIO %d (%s)", 
                     channels[UM_OC_CHANNEL_2].gpio_num,
                     channels[UM_OC_CHANNEL_2].state == UM_OC_STATE_ON ? "ON" : "OFF");
        }
    }
#endif
    
    // 4. Если в NVS не было данных, сохраняем текущие состояния
#if UM_FEATURE_ENABLED(OC1) || UM_FEATURE_ENABLED(OC2)
    if (saved_states < 0) {
        um_opencollectors_save_to_nvs();
        ESP_LOGI(TAG, "Initial states saved to NVS");
    }
#endif
    
    return ret;
}

esp_err_t um_opencollectors_save_to_nvs(void) {
    int8_t states = 0;
    
#if UM_FEATURE_ENABLED(OC1)
    if (channels[UM_OC_CHANNEL_1].enabled) {
        if (channels[UM_OC_CHANNEL_1].state == UM_OC_STATE_ON) {
            states |= OC1_STATE_MASK;
        }
    }
#endif

#if UM_FEATURE_ENABLED(OC2)
    if (channels[UM_OC_CHANNEL_2].enabled) {
        if (channels[UM_OC_CHANNEL_2].state == UM_OC_STATE_ON) {
            states |= OC2_STATE_MASK;
        }
    }
#endif
    
    return um_nvs_write_i8(UM_NVS_KEY_OPENCOLLECTORS, states);
}

esp_err_t um_opencollectors_set(um_oc_channel_t channel, um_oc_state_t state) {
    if (channel == UM_OC_CHANNEL_1) {
#if UM_FEATURE_ENABLED(OC1)
        if (!channels[UM_OC_CHANNEL_1].enabled) {
            return ESP_ERR_INVALID_STATE;
        }
        if (channels[UM_OC_CHANNEL_1].state == state) {
            return ESP_OK;
        }
        channels[UM_OC_CHANNEL_1].state = state;
        gpio_set_level(channels[UM_OC_CHANNEL_1].gpio_num, state_to_level(state));
        ESP_LOGI(TAG, "OC1 set to %s", state == UM_OC_STATE_ON ? "ON" : "OFF");

        um_opencollectors_save_to_nvs();

        return ESP_OK;
#else
        ESP_LOGE(TAG, "OC1 not enabled in Kconfig");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (channel == UM_OC_CHANNEL_2) {
#if UM_FEATURE_ENABLED(OC2)
        if (!channels[UM_OC_CHANNEL_2].enabled) {
            return ESP_ERR_INVALID_STATE;
        }
        if (channels[UM_OC_CHANNEL_2].state == state) {
            return ESP_OK;
        }
        channels[UM_OC_CHANNEL_2].state = state;
        gpio_set_level(channels[UM_OC_CHANNEL_2].gpio_num, state_to_level(state));
        ESP_LOGI(TAG, "OC2 set to %s", state == UM_OC_STATE_ON ? "ON" : "OFF");
        return ESP_OK;
#else
        ESP_LOGE(TAG, "OC2 not enabled in Kconfig");
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    
    return ESP_ERR_INVALID_ARG;
}

esp_err_t um_opencollectors_get(um_oc_channel_t channel, um_oc_state_t* state) {
    if (channel == UM_OC_CHANNEL_1) {
#if UM_FEATURE_ENABLED(OC1)
        if (!channels[UM_OC_CHANNEL_1].enabled) {
            return ESP_ERR_INVALID_STATE;
        }
        *state = channels[UM_OC_CHANNEL_1].state;
        return ESP_OK;
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (channel == UM_OC_CHANNEL_2) {
#if UM_FEATURE_ENABLED(OC2)
        if (!channels[UM_OC_CHANNEL_2].enabled) {
            return ESP_ERR_INVALID_STATE;
        }
        *state = channels[UM_OC_CHANNEL_2].state;
        return ESP_OK;
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    
    return ESP_ERR_INVALID_ARG;
}

esp_err_t um_opencollectors_toggle(um_oc_channel_t channel) {
    um_oc_state_t current;
    esp_err_t ret = um_opencollectors_get(channel, &current);
    if (ret != ESP_OK) return ret;
    
    return um_opencollectors_set(channel, 
        current == UM_OC_STATE_ON ? UM_OC_STATE_OFF : UM_OC_STATE_ON);
}

esp_err_t um_opencollectors_all_off(void) {
    esp_err_t ret1 = ESP_OK, ret2 = ESP_OK;
    
#if UM_FEATURE_ENABLED(OC1)
    if (channels[UM_OC_CHANNEL_1].enabled && 
        channels[UM_OC_CHANNEL_1].state == UM_OC_STATE_ON) {
        ret1 = um_opencollectors_set(UM_OC_CHANNEL_1, UM_OC_STATE_OFF);
    }
#endif

#if UM_FEATURE_ENABLED(OC2)
    if (channels[UM_OC_CHANNEL_2].enabled && 
        channels[UM_OC_CHANNEL_2].state == UM_OC_STATE_ON) {
        ret2 = um_opencollectors_set(UM_OC_CHANNEL_2, UM_OC_STATE_OFF);
    }
#endif
    
    return (ret1 == ESP_OK && ret2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

#endif