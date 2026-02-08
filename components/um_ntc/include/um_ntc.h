#ifndef UM_NTC_H
#define UM_NTC_H

#include <stdbool.h>
#include "esp_err.h"
#include "ntc_driver.h"
#include "base_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief NTC channel states
     */
    typedef enum
    {
        UM_NTC_STATE_DISABLED = 0, /**< Channel is disabled */
        UM_NTC_STATE_ENABLED,      /**< Channel is enabled and active */
        UM_NTC_STATE_ERROR         /**< Channel has error */
    } um_ntc_state_t;

    /**
     * @brief NTC channel IDs
     */
    typedef enum
    {
        UM_NTC_CHANNEL_1 = 0,
        UM_NTC_CHANNEL_2
    } um_ntc_channel_id_t;

#if UM_FEATURE_ENABLED(NTC1)
#define UM_NTC_1 CONFIG_UM_CFG_NTC1_ADC_CHANNEL
#endif

#if UM_FEATURE_ENABLED(NTC2)
#define UM_NTC_2 CONFIG_UM_CFG_NTC2_ADC_CHANNEL
#endif

    /**
     * @brief Initialize NTC system
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_ntc_init(adc_oneshot_unit_handle_t *adc_handle);

    /**
     * @brief Read temperature from NTC channel
     *
     * @param channel_id Channel ID (UM_NTC_CHANNEL_1 or UM_NTC_CHANNEL_2)
     * @param temperature Pointer to store temperature value
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_ntc_read_temperature(um_ntc_channel_id_t channel_id, float *temperature);

    /**
     * @brief Get current channel state
     *
     * @param channel_id Channel ID
     * @return Current channel state
     */
    um_ntc_state_t um_ntc_get_state(um_ntc_channel_id_t channel_id);

    /**
     * @brief Enable or disable NTC channel
     *
     * @param channel_id Channel ID
     * @param enable true to enable, false to disable
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_ntc_set_channel_enabled(um_ntc_channel_id_t channel_id, bool enable);

    /**
     * @brief Get last temperature reading
     *
     * @param channel_id Channel ID
     * @param temperature Pointer to store temperature
     * @return ESP_OK if temperature is valid, error code otherwise
     */
    esp_err_t um_ntc_get_last_temperature(um_ntc_channel_id_t channel_id, float *temperature);

    /**
     * @brief Enable or disable all NTC channels
     *
     * @param enable true to enable, false to disable
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_ntc_set_all_enabled(bool enable);

    /**
     * @brief Read temperatures from all enabled channels
     *
     * @param temp1 Pointer to store channel 1 temperature (can be NULL)
     * @param temp2 Pointer to store channel 2 temperature (can be NULL)
     * @return Bitmask of successful reads (bit 0 = channel 1, bit 1 = channel 2)
     */
    uint8_t um_ntc_read_all(float *temp1, float *temp2);

#ifdef __cplusplus
}
#endif

#endif // UM_NTC_H