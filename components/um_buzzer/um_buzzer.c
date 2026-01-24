/**
 * @file um_buzzer.c
 * @brief Simple buzzer implementation
 */

#include "um_buzzer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char* TAG = "um_buzzer";

#if defined(CONFIG_UM_FEATURE_BUZZER)

static struct {
    bool initialized;
    int gpio_num;
    um_buzzer_state_t state;
} buzzer = {
    .initialized = false,
    .gpio_num = -1,
    .state = UM_BUZZER_OFF
};

// Convert state to GPIO level (assuming active HIGH buzzer)
static inline int state_to_level(um_buzzer_state_t state) {
    return (state == UM_BUZZER_ON) ? 1 : 0;
}

esp_err_t um_buzzer_init(void) {
    if (buzzer.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    buzzer.gpio_num = CONFIG_UM_CFG_BUZZER_GPIO;
    
    if (buzzer.gpio_num < 0) {
        ESP_LOGE(TAG, "Invalid GPIO: %d", buzzer.gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << buzzer.gpio_num),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", 
                 buzzer.gpio_num, esp_err_to_name(ret));
        return ret;
    }
    
    // Start with OFF state
    buzzer.state = UM_BUZZER_OFF;
    gpio_set_level(buzzer.gpio_num, state_to_level(UM_BUZZER_OFF));
    
    buzzer.initialized = true;
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", buzzer.gpio_num);
    
    return ESP_OK;
}

esp_err_t um_buzzer_set(um_buzzer_state_t state) {
    if (!buzzer.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (buzzer.state == state) {
        return ESP_OK; // Already in desired state
    }
    
    buzzer.state = state;
    gpio_set_level(buzzer.gpio_num, state_to_level(state));
    
    ESP_LOGI(TAG, "Buzzer set to %s", 
             state == UM_BUZZER_ON ? "ON" : "OFF");
    
    return ESP_OK;
}

esp_err_t um_buzzer_get(um_buzzer_state_t* state) {
    if (!buzzer.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *state = buzzer.state;
    return ESP_OK;
}

esp_err_t um_buzzer_toggle(void) {
    if (!buzzer.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    um_buzzer_state_t new_state = (buzzer.state == UM_BUZZER_ON) 
                                  ? UM_BUZZER_OFF 
                                  : UM_BUZZER_ON;
    
    return um_buzzer_set(new_state);
}

esp_err_t um_buzzer_beep(uint8_t beeps, uint16_t on_time_ms, uint16_t off_time_ms) {
    if (!buzzer.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (beeps == 0) {
        return ESP_OK;
    }
    
    for (uint8_t i = 0; i < beeps; i++) {
        // Beep ON
        um_buzzer_set(UM_BUZZER_ON);
        vTaskDelay(pdMS_TO_TICKS(on_time_ms));
        
        // Beep OFF (except for last beep)
        if (i < beeps - 1) {
            um_buzzer_set(UM_BUZZER_OFF);
            vTaskDelay(pdMS_TO_TICKS(off_time_ms));
        }
    }
    
    // Always end with OFF
    um_buzzer_set(UM_BUZZER_OFF);
    
    ESP_LOGI(TAG, "Beep pattern: %d beeps (on=%dms, off=%dms)", 
             beeps, on_time_ms, off_time_ms);
    
    return ESP_OK;
}

#endif // CONFIG_UM_FEATURE_BUZZER