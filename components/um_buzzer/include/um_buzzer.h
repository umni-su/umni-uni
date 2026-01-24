/**
 * @file um_buzzer.h
 * @brief Simple buzzer control for ESP-IDF
 */

#ifndef UM_BUZZER_H
#define UM_BUZZER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_UM_FEATURE_BUZZER)

/**
 * @brief Buzzer state
 */
typedef enum {
    UM_BUZZER_OFF = 0,
    UM_BUZZER_ON = 1
} um_buzzer_state_t;

/**
 * @brief Initialize buzzer
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_buzzer_init(void);

/**
 * @brief Set buzzer state
 * 
 * @param state Desired state
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_buzzer_set(um_buzzer_state_t state);

/**
 * @brief Get current buzzer state
 * 
 * @param[out] state Current state
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_buzzer_get(um_buzzer_state_t* state);

/**
 * @brief Toggle buzzer state
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_buzzer_toggle(void);

/**
 * @brief Beep pattern (blocking)
 * 
 * @param beeps Number of beeps
 * @param on_time_ms Beep duration in ms
 * @param off_time_ms Pause between beeps in ms
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_buzzer_beep(uint8_t beeps, uint16_t on_time_ms, uint16_t off_time_ms);

#endif // CONFIG_UM_FEATURE_BUZZER

#ifdef __cplusplus
}
#endif

#endif // UM_BUZZER_H