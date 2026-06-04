/*
 * um_ot.h - OpenTherm Communication Library for ESP-IDF
 */

#ifndef UM_OTRM_H
#define UM_OTRM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        UM_OTRM_RESPONSE_NONE,
        UM_OTRM_RESPONSE_SUCCESS,
        UM_OTRM_RESPONSE_INVALID,
        UM_OTRM_RESPONSE_TIMEOUT
    } um_otrm_response_status_t;

    typedef enum
    {
        UM_OTRM_MSG_READ_DATA = 0b000,
        UM_OTRM_MSG_WRITE_DATA = 0b001,
        UM_OTRM_MSG_INVALID_DATA = 0b010,
        UM_OTRM_MSG_RESERVED = 0b011,
        UM_OTRM_MSG_READ_ACK = 0b100,
        UM_OTRM_MSG_WRITE_ACK = 0b101,
        UM_OTRM_MSG_DATA_INVALID = 0b110,
        UM_OTRM_MSG_UNKNOWN_DATA_ID = 0b111
    } um_otrm_message_type_t;

    typedef enum
    {
        UM_OTRM_ID_STATUS = 0,
        UM_OTRM_ID_TSET = 1,
        UM_OTRM_ID_TBOILER = 25,
        UM_OTRM_ID_TDHW = 26,
        UM_OTRM_ID_TRET = 28,
        UM_OTRM_ID_REL_MOD_LEVEL = 17,
        UM_OTRM_ID_CH_PRESSURE = 18,
        UM_OTRM_ID_ASF_FLAGS = 5,
        UM_OTRM_ID_TDHW_SET = 56
    } um_otrm_message_id_t;

    typedef enum
    {
        UM_OTRM_STATE_NOT_INITIALIZED,
        UM_OTRM_STATE_READY,
        UM_OTRM_STATE_DELAY,
        UM_OTRM_STATE_REQUEST_SENDING,
        UM_OTRM_STATE_RESPONSE_WAITING,
        UM_OTRM_STATE_RESPONSE_START_BIT,
        UM_OTRM_STATE_RESPONSE_RECEIVING,
        UM_OTRM_STATE_RESPONSE_READY,
        UM_OTRM_STATE_RESPONSE_INVALID
    } um_otrm_state_t;

    typedef struct um_ot um_otrm_t;

    typedef void (*um_otrm_response_cb_t)(unsigned long response, um_otrm_response_status_t status, void *user_data);

    /* Create/destroy */
    um_otrm_t *um_otrm_create(int in_pin, int out_pin, bool is_slave);
    void um_otrm_destroy(um_otrm_t *ot);

    /* Init/deinit */
    void um_otrm_begin(um_otrm_t *ot, um_otrm_response_cb_t callback, void *user_data);
    void um_otrm_end(um_otrm_t *ot);

    /* State */
    um_otrm_state_t um_otrm_get_state(um_otrm_t *ot);
    bool um_otrm_is_ready(um_otrm_t *ot);

    /* Send requests */
    unsigned long um_otrm_send_request(um_otrm_t *ot, unsigned long request);
    bool um_otrm_send_request_async(um_otrm_t *ot, unsigned long request);
    bool um_otrm_send_response(um_otrm_t *ot, unsigned long request);

    /* Get results */
    unsigned long um_otrm_get_last_response(um_otrm_t *ot);
    um_otrm_response_status_t um_otrm_get_last_response_status(um_otrm_t *ot);

    /* Process */
    void um_otrm_process(um_otrm_t *ot);
    void um_otrm_handle_interrupt(um_otrm_t *ot);

    /* Static utility functions */
    bool um_otrm_parity(unsigned long frame);
    um_otrm_message_type_t um_otrm_get_message_type(unsigned long message);
    um_otrm_message_id_t um_otrm_get_data_id(unsigned long frame);
    const char *um_otrm_response_status_to_string(um_otrm_response_status_t status);
    const char *um_otrm_message_type_to_string(um_otrm_message_type_t type);
    bool um_otrm_is_valid_request(unsigned long request);
    bool um_otrm_is_valid_response(unsigned long response);

    /* Build requests/responses */
    unsigned long um_otrm_build_request(um_otrm_message_type_t type, um_otrm_message_id_t id, unsigned int data);
    unsigned long um_otrm_build_response(um_otrm_message_type_t type, um_otrm_message_id_t id, unsigned int data);

    /* Request builders */
    unsigned long um_otrm_build_set_boiler_status_request(bool enable_central_heating, bool enable_hot_water,
                                                          bool enable_cooling, bool enable_outside_temp_comp,
                                                          bool enable_central_heating2);
    unsigned long um_otrm_build_set_boiler_temperature_request(float temperature);
    unsigned long um_otrm_build_get_boiler_temperature_request(void);

    /* Response parsers */
    bool um_otrm_is_fault(unsigned long response);
    bool um_otrm_is_central_heating_active(unsigned long response);
    bool um_otrm_is_hot_water_active(unsigned long response);
    bool um_otrm_is_flame_on(unsigned long response);
    bool um_otrm_is_cooling_active(unsigned long response);
    bool um_otrm_is_diagnostic(unsigned long response);
    uint16_t um_otrm_get_uint(unsigned long response);
    float um_otrm_get_float(unsigned long response);
    unsigned int um_otrm_temperature_to_data(float temperature);

    /* Basic requests */
    unsigned long um_otrm_set_boiler_status(um_otrm_t *ot, bool enable_central_heating, bool enable_hot_water,
                                            bool enable_cooling, bool enable_outside_temp_comp,
                                            bool enable_central_heating2);
    bool um_otrm_set_boiler_temperature(um_otrm_t *ot, float temperature);
    float um_otrm_get_boiler_temperature(um_otrm_t *ot);
    float um_otrm_get_return_temperature(um_otrm_t *ot);
    bool um_otrm_set_dhw_setpoint(um_otrm_t *ot, float temperature);
    float um_otrm_get_dhw_temperature(um_otrm_t *ot);
    float um_otrm_get_modulation(um_otrm_t *ot);
    float um_otrm_get_pressure(um_otrm_t *ot);
    unsigned char um_otrm_get_fault(um_otrm_t *ot);

#ifdef __cplusplus
}
#endif

#endif /** UM_OTRM_H */