#ifndef UM_ADC_COMMON_H
#define UM_ADC_COMMON_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Объявляем extern handle для доступа напрямую
    extern adc_oneshot_unit_handle_t um_adc_common_handle;

    /**
     * @brief Initialize common ADC system
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_common_init(void);

    /**
     * @brief Get ADC handle
     *
     * @return ADC oneshot handle or NULL if not initialized
     */
    adc_oneshot_unit_handle_t um_adc_common_get_handle(void);

    /**
     * @brief Deinitialize common ADC system
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_adc_common_deinit(void);

    /**
     * @brief Check if ADC system is initialized
     *
     * @return true if initialized, false otherwise
     */
    bool um_adc_common_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // UM_ADC_COMMON_H