/**
 * @file um_dio.h
 * @brief Digital Input/Output module for PCF8574
 * @version 1.0.0
 */

#ifndef UM_DIO_H
#define UM_DIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DO_1 = CONFIG_UM_CFG_OUT1_INDEX,
    DO_2 = CONFIG_UM_CFG_OUT2_INDEX,
    DO_3 = CONFIG_UM_CFG_OUT3_INDEX,
    DO_4 = CONFIG_UM_CFG_OUT4_INDEX,
    DO_5 = CONFIG_UM_CFG_OUT5_INDEX,
    DO_6 = CONFIG_UM_CFG_OUT6_INDEX,
    DO_7 = CONFIG_UM_CFG_OUT7_INDEX,
    DO_8 = CONFIG_UM_CFG_OUT8_INDEX
} um_do_port_index_t;

typedef enum
{
    DO_HIGH = 1,
    DO_LOW = 0
} um_do_level_t;

/**
 * @brief Initialize DIO module
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_init(void);

/**
 * @brief Get input pin state
 * 
 * @param input_idx Input index (1-6)
 * @param[out] state Pointer to store state (true = high, false = low)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_get_input(uint8_t input_idx, bool *state);

/**
 * @brief Get all input states as bitmask
 * 
 * @param[out] states Bitmask of all inputs (bit 0 = input 1, etc.)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_get_all_inputs(uint8_t *states);

/**
 * @brief Set output pin state
 * 
 * @param output_idx Output index (1-8)
 * @param state true = high, false = low
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_set_output(um_do_port_index_t output_idx, um_do_level_t level);

/**
 * @brief Get output pin state
 * 
 * @param output_idx Output index (1-8)
 * @param[out] state Pointer to store state
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_get_output(uint8_t output_idx, bool *state);

/**
 * @brief Set all outputs from bitmask
 * 
 * @param states Bitmask of outputs (bit 0 = output 1, etc.)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_set_all_outputs(uint8_t states);

/**
 * @brief Get all outputs as bitmask
 * 
 * @param[out] states Bitmask of all outputs
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_get_all_outputs(uint8_t *states);

/**
 * @brief Toggle output pin
 * 
 * @param output_idx Output index (1-8)
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_dio_toggle_output(uint8_t output_idx);

/**
 * @brief Deinitialize DIO module
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_dio_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // UM_DIO_H