/**
 * @file um_alarm.h
 * @brief Alarm input with interrupt handling
 */

#ifndef UM_ALARM_H
#define UM_ALARM_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_UM_FEATURE_ALARM)

/**
 * @brief Alarm event callback type
 */
typedef void (*um_alarm_callback_t)(bool state, void* user_data);

/**
 * @brief Alarm edge detection mode
 */
typedef enum {
    UM_ALARM_EDGE_FALLING = 0,   /**< Falling edge (HIGH -> LOW) */
    UM_ALARM_EDGE_RISING = 1,    /**< Rising edge (LOW -> HIGH) */
    UM_ALARM_EDGE_BOTH = 2       /**< Both edges */
} um_alarm_edge_t;

/**
 * @brief Initialize alarm input
 * 
 * @param edge Edge detection mode
 * @param pull_up Enable internal pull-up
 * @param pull_down Enable internal pull-down
 * @param debounce_ms Debounce timeout ms
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_init(um_alarm_edge_t edge, bool pull_up, bool pull_down, int debounce_ms);

/**
 * @brief Set callback for alarm events
 * 
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_set_callback(um_alarm_callback_t callback, void* user_data);

/**
 * @brief Get current alarm state
 * 
 * @param[out] state Current state (true = HIGH, false = LOW)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_get_state(bool* state);

/**
 * @brief Get alarm trigger count since init
 * 
 * @param[out] count Trigger count
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_get_count(uint32_t* count);

/**
 * @brief Reset alarm trigger count
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_reset_count(void);

/**
 * @brief Deinitialize alarm input
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_deinit(void);

/**
 * @brief Set debounce timeout
 * 
 * @param debounce_ms Debounce ms
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_alarm_set_debounce(int debounce_ms);

#endif // CONFIG_UM_FEATURE_ALARM

#ifdef __cplusplus
}
#endif

#endif // UM_ALARM_H