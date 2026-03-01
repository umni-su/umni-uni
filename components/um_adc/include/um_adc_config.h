#ifndef UM_ADC_CONFIG_H
#define UM_ADC_CONFIG_H

#include "um_adc.h"
#include "cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Maximum number of ADC channels
 */
#define ADC_MAX_CHANNELS 2

    /**
     * @brief ADC channel configuration structure
     */
    typedef struct
    {
        uint8_t channel_id; /**< Channel ID (0 for CHANNEL_1, 1 for CHANNEL_2) */
        char label[32];     /**< Human-readable label for the channel */
        bool active;        /**< Whether channel is active */
        float scale_factor; /**< Scale factor for voltage conversion (optional) */
        float offset;       /**< Offset for voltage conversion (optional) */
        // Note: ADC channel number is fixed from Kconfig, not stored in config
    } um_adc_channel_config_t;

    /**
     * @brief Load ADC channel configurations from file
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_config_load(void);

    /**
     * @brief Save current ADC channel configurations to file
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_config_save(void);

    /**
     * @brief Apply loaded configuration to ADC channels
     *
     * Enables/disables channels based on active flag
     */
    void um_adc_config_apply(void);

    /**
     * @brief Get ADC configuration as JSON string
     *
     * @return char* JSON string (must be freed by caller) or NULL on error
     */
    char *um_adc_config_read(void);

    /**
     * @brief Update configuration for a specific ADC channel
     *
     * @param channel_id Channel ID (0 for CHANNEL_1, 1 for CHANNEL_2)
     * @param config New configuration
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_config_update(uint8_t channel_id, const um_adc_channel_config_t *config);

    /**
     * @brief Get configuration for a specific ADC channel
     *
     * @param channel_id Channel ID (0 for CHANNEL_1, 1 for CHANNEL_2)
     * @return const um_adc_channel_config_t* Pointer to configuration or NULL
     */
    const um_adc_channel_config_t *um_adc_config_get(uint8_t channel_id);

    /**
     * @brief Create default configuration based on available channels
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_config_create_default(void);

#ifdef __cplusplus
}
#endif

#endif // UM_ADC_CONFIG_H