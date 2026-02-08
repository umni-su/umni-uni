#ifndef UM_ADC_H
#define UM_ADC_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "base_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief ADC channel states
     */
    typedef enum
    {
        UM_ADC_STATE_DISABLED = 0, /**< Channel is disabled */
        UM_ADC_STATE_ENABLED,      /**< Channel is enabled and active */
        UM_ADC_STATE_ERROR         /**< Channel has error */
    } um_adc_state_t;

    /**
     * @brief ADC channel IDs
     */
    typedef enum
    {
        UM_ADC_CHANNEL_1 = 0,
        UM_ADC_CHANNEL_2
    } um_adc_channel_id_t;

#if UM_FEATURE_ENABLED(AI1)
#define UM_ADC_1 CONFIG_UM_CFG_AI1_ADC_CHANNEL
#endif

#if UM_FEATURE_ENABLED(AI2)
#define UM_ADC_2 CONFIG_UM_CFG_AI2_ADC_CHANNEL
#endif

    /**
     * @brief Initialize ADC system
     *
     * @param adc_handle Pointer to shared ADC handle (from um_adc_common)
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_init(adc_oneshot_unit_handle_t *adc_handle);

    /**
     * @brief Read raw value from ADC channel
     *
     * @param channel_id Channel ID (UM_ADC_CHANNEL_1 or UM_ADC_CHANNEL_2)
     * @param raw_value Pointer to store raw ADC value
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_read_raw(um_adc_channel_id_t channel_id, int *raw_value);

    /**
     * @brief Get current channel state
     *
     * @param channel_id Channel ID
     * @return Current channel state
     */
    um_adc_state_t um_adc_get_state(um_adc_channel_id_t channel_id);

    /**
     * @brief Enable or disable ADC channel
     *
     * @param channel_id Channel ID
     * @param enable true to enable, false to disable
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_set_channel_enabled(um_adc_channel_id_t channel_id, bool enable);

    /**
     * @brief Get last raw reading
     *
     * @param channel_id Channel ID
     * @param raw_value Pointer to store raw value
     * @return ESP_OK if value is valid, error code otherwise
     */
    esp_err_t um_adc_get_last_raw(um_adc_channel_id_t channel_id, int *raw_value);

    /**
     * @brief Enable or disable all ADC channels
     *
     * @param enable true to enable, false to disable
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_set_all_enabled(bool enable);

    /**
     * @brief Read raw values from all enabled channels
     *
     * @param raw1 Pointer to store channel 1 raw value (can be NULL)
     * @param raw2 Pointer to store channel 2 raw value (can be NULL)
     * @return Bitmask of successful reads (bit 0 = channel 1, bit 1 = channel 2)
     */
    uint8_t um_adc_read_all_raw(int *raw1, int *raw2);

    /**
     * @brief Deinitialize ADC system
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // UM_ADC_H