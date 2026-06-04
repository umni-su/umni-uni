/*
 * um_ot.c - OpenTherm Communication Library for ESP-IDF
 */

#include "um_ot.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include <string.h>
#include <stdlib.h>

#define BIT_TIME_US 500
#define TIMEOUT_US 1000000
#define DELAY_MASTER_US 100000
#define DELAY_SLAVE_US 20000
#define BOILER_ACTIVATE_DELAY_MS 1000
#define START_BIT_WINDOW_US 750

struct um_ot
{
    int in_pin;
    int out_pin;
    bool is_slave;

    volatile um_otrm_state_t state;
    volatile unsigned long response;
    volatile um_otrm_response_status_t response_status;
    volatile int64_t response_timestamp;
    volatile uint8_t response_bit_index;

    um_otrm_response_cb_t callback;
    void *user_data;

    bool intr_initialized;
    portMUX_TYPE spinlock;
};

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    um_otrm_t *ot = (um_otrm_t *)arg;
    um_otrm_handle_interrupt(ot);
}

static void set_active_state(um_otrm_t *ot)
{
    gpio_set_level(ot->out_pin, 0);
}

static void set_idle_state(um_otrm_t *ot)
{
    gpio_set_level(ot->out_pin, 1);
}

static void activate_boiler(um_otrm_t *ot)
{
    set_idle_state(ot);
    vTaskDelay(pdMS_TO_TICKS(BOILER_ACTIVATE_DELAY_MS));
}

static void send_bit(um_otrm_t *ot, bool high)
{
    if (high)
    {
        set_active_state(ot);
    }
    else
    {
        set_idle_state(ot);
    }
    esp_rom_delay_us(BIT_TIME_US);
    if (high)
    {
        set_idle_state(ot);
    }
    else
    {
        set_active_state(ot);
    }
    esp_rom_delay_us(BIT_TIME_US);
}

um_otrm_t *um_otrm_create(int in_pin, int out_pin, bool is_slave)
{
    um_otrm_t *ot = (um_otrm_t *)calloc(1, sizeof(um_otrm_t));
    if (!ot)
        return NULL;

    ot->in_pin = in_pin;
    ot->out_pin = out_pin;
    ot->is_slave = is_slave;
    ot->state = UM_OTRM_STATE_NOT_INITIALIZED;
    ot->response = 0;
    ot->response_status = UM_OTRM_RESPONSE_NONE;
    ot->response_timestamp = 0;
    ot->callback = NULL;
    ot->user_data = NULL;
    ot->intr_initialized = false;
    ot->spinlock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    return ot;
}

void um_otrm_destroy(um_otrm_t *ot)
{
    if (ot)
    {
        um_otrm_end(ot);
        free(ot);
    }
}

um_otrm_state_t um_otrm_get_state(um_otrm_t *ot)
{
    um_otrm_state_t state;
    portENTER_CRITICAL(&ot->spinlock);
    state = ot->state;
    portEXIT_CRITICAL(&ot->spinlock);
    return state;
}

void um_otrm_begin(um_otrm_t *ot, um_otrm_response_cb_t callback, void *user_data)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ot->in_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << ot->out_pin);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    ot->callback = callback;
    ot->user_data = user_data;

    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(ot->in_pin, gpio_isr_handler, ot);
    ot->intr_initialized = true;

    activate_boiler(ot);
    ot->state = UM_OTRM_STATE_READY;
}

bool um_otrm_is_ready(um_otrm_t *ot)
{
    return ot->state == UM_OTRM_STATE_READY;
}

bool um_otrm_send_request_async(um_otrm_t *ot, unsigned long request)
{
    portENTER_CRITICAL(&ot->spinlock);
    bool ready = (ot->state == UM_OTRM_STATE_READY);

    if (!ready)
    {
        portEXIT_CRITICAL(&ot->spinlock);
        return false;
    }

    ot->state = UM_OTRM_STATE_REQUEST_SENDING;
    ot->response = 0;
    ot->response_status = UM_OTRM_RESPONSE_NONE;
    portEXIT_CRITICAL(&ot->spinlock);

    send_bit(ot, true);
    for (int i = 31; i >= 0; i--)
    {
        send_bit(ot, (request >> i) & 1);
    }
    send_bit(ot, true);
    set_idle_state(ot);

    portENTER_CRITICAL(&ot->spinlock);
    ot->response_timestamp = esp_timer_get_time();
    ot->state = UM_OTRM_STATE_RESPONSE_WAITING;
    portEXIT_CRITICAL(&ot->spinlock);

    return true;
}

unsigned long um_otrm_send_request(um_otrm_t *ot, unsigned long request)
{
    if (!um_otrm_send_request_async(ot, request))
    {
        return 0;
    }

    while (!um_otrm_is_ready(ot))
    {
        um_otrm_process(ot);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ot->response;
}

bool um_otrm_send_response(um_otrm_t *ot, unsigned long request)
{
    portENTER_CRITICAL(&ot->spinlock);
    bool ready = (ot->state == UM_OTRM_STATE_READY);

    if (!ready)
    {
        portEXIT_CRITICAL(&ot->spinlock);
        return false;
    }

    ot->state = UM_OTRM_STATE_REQUEST_SENDING;
    ot->response = 0;
    ot->response_status = UM_OTRM_RESPONSE_NONE;
    portEXIT_CRITICAL(&ot->spinlock);

    send_bit(ot, true);
    for (int i = 31; i >= 0; i--)
    {
        send_bit(ot, (request >> i) & 1);
    }
    send_bit(ot, true);
    set_idle_state(ot);

    portENTER_CRITICAL(&ot->spinlock);
    ot->state = UM_OTRM_STATE_READY;
    portEXIT_CRITICAL(&ot->spinlock);

    return true;
}

unsigned long um_otrm_get_last_response(um_otrm_t *ot)
{
    unsigned long resp;
    portENTER_CRITICAL(&ot->spinlock);
    resp = ot->response;
    portEXIT_CRITICAL(&ot->spinlock);
    return resp;
}

um_otrm_response_status_t um_otrm_get_last_response_status(um_otrm_t *ot)
{
    um_otrm_response_status_t status;
    portENTER_CRITICAL(&ot->spinlock);
    status = ot->response_status;
    portEXIT_CRITICAL(&ot->spinlock);
    return status;
}

void um_otrm_handle_interrupt(um_otrm_t *ot)
{
    int64_t new_ts = esp_timer_get_time();

    portENTER_CRITICAL_ISR(&ot->spinlock);

    if (ot->state == UM_OTRM_STATE_READY)
    {
        if (ot->is_slave && gpio_get_level(ot->in_pin) == 1)
        {
            ot->state = UM_OTRM_STATE_RESPONSE_WAITING;
        }
        else
        {
            portEXIT_CRITICAL_ISR(&ot->spinlock);
            return;
        }
    }

    if (ot->state == UM_OTRM_STATE_RESPONSE_WAITING)
    {
        if (gpio_get_level(ot->in_pin) == 1)
        {
            ot->state = UM_OTRM_STATE_RESPONSE_START_BIT;
            ot->response_timestamp = new_ts;
        }
        else
        {
            ot->state = UM_OTRM_STATE_RESPONSE_INVALID;
            ot->response_timestamp = new_ts;
        }
    }
    else if (ot->state == UM_OTRM_STATE_RESPONSE_START_BIT)
    {
        if ((new_ts - ot->response_timestamp < START_BIT_WINDOW_US) && gpio_get_level(ot->in_pin) == 0)
        {
            ot->state = UM_OTRM_STATE_RESPONSE_RECEIVING;
            ot->response_timestamp = new_ts;
            ot->response_bit_index = 0;
        }
        else
        {
            ot->state = UM_OTRM_STATE_RESPONSE_INVALID;
            ot->response_timestamp = new_ts;
        }
    }
    else if (ot->state == UM_OTRM_STATE_RESPONSE_RECEIVING)
    {
        if ((new_ts - ot->response_timestamp) > START_BIT_WINDOW_US)
        {
            if (ot->response_bit_index < 32)
            {
                ot->response = (ot->response << 1) | !gpio_get_level(ot->in_pin);
                ot->response_timestamp = new_ts;
                ot->response_bit_index++;
            }
            else
            {
                ot->state = UM_OTRM_STATE_RESPONSE_READY;
                ot->response_timestamp = new_ts;
            }
        }
    }

    portEXIT_CRITICAL_ISR(&ot->spinlock);
}

static void process_response(um_otrm_t *ot)
{
    if (ot->callback)
    {
        ot->callback(ot->response, ot->response_status, ot->user_data);
    }
}

void um_otrm_process(um_otrm_t *ot)
{
    portENTER_CRITICAL(&ot->spinlock);
    um_otrm_state_t state = ot->state;
    int64_t timestamp = ot->response_timestamp;
    portEXIT_CRITICAL(&ot->spinlock);

    if (state == UM_OTRM_STATE_READY)
        return;

    int64_t new_ts = esp_timer_get_time();
    int64_t timeout = TIMEOUT_US;

    if (state != UM_OTRM_STATE_NOT_INITIALIZED && state != UM_OTRM_STATE_DELAY && (new_ts - timestamp) > timeout)
    {
        portENTER_CRITICAL(&ot->spinlock);
        ot->state = UM_OTRM_STATE_READY;
        ot->response_status = UM_OTRM_RESPONSE_TIMEOUT;
        portEXIT_CRITICAL(&ot->spinlock);
        process_response(ot);
    }
    else if (state == UM_OTRM_STATE_RESPONSE_INVALID)
    {
        portENTER_CRITICAL(&ot->spinlock);
        ot->state = UM_OTRM_STATE_DELAY;
        ot->response_status = UM_OTRM_RESPONSE_INVALID;
        portEXIT_CRITICAL(&ot->spinlock);
        process_response(ot);
    }
    else if (state == UM_OTRM_STATE_RESPONSE_READY)
    {
        portENTER_CRITICAL(&ot->spinlock);
        ot->state = UM_OTRM_STATE_DELAY;
        bool valid = ot->is_slave ? um_otrm_is_valid_request(ot->response) : um_otrm_is_valid_response(ot->response);
        ot->response_status = valid ? UM_OTRM_RESPONSE_SUCCESS : UM_OTRM_RESPONSE_INVALID;
        portEXIT_CRITICAL(&ot->spinlock);
        process_response(ot);
    }
    else if (state == UM_OTRM_STATE_DELAY)
    {
        int64_t delay_time = ot->is_slave ? DELAY_SLAVE_US : DELAY_MASTER_US;
        if ((new_ts - timestamp) > delay_time)
        {
            portENTER_CRITICAL(&ot->spinlock);
            ot->state = UM_OTRM_STATE_READY;
            portEXIT_CRITICAL(&ot->spinlock);
        }
    }
}

void um_otrm_end(um_otrm_t *ot)
{
    if (ot->intr_initialized)
    {
        gpio_isr_handler_remove(ot->in_pin);
        ot->intr_initialized = false;
    }
    ot->state = UM_OTRM_STATE_NOT_INITIALIZED;
}

/* Static utility functions */

bool um_otrm_parity(unsigned long frame)
{
    uint8_t p = 0;
    while (frame > 0)
    {
        if (frame & 1)
            p++;
        frame = frame >> 1;
    }
    return (p & 1);
}

um_otrm_message_type_t um_otrm_get_message_type(unsigned long message)
{
    return (um_otrm_message_type_t)((message >> 28) & 7);
}

um_otrm_message_id_t um_otrm_get_data_id(unsigned long frame)
{
    return (um_otrm_message_id_t)((frame >> 16) & 0xFF);
}

unsigned long um_otrm_build_request(um_otrm_message_type_t type, um_otrm_message_id_t id, unsigned int data)
{
    unsigned long request = data;
    if (type == UM_OTRM_MSG_WRITE_DATA)
    {
        request |= 1ul << 28;
    }
    request |= ((unsigned long)id) << 16;
    if (um_otrm_parity(request))
    {
        request |= (1ul << 31);
    }
    return request;
}

unsigned long um_otrm_build_response(um_otrm_message_type_t type, um_otrm_message_id_t id, unsigned int data)
{
    unsigned long response = data;
    response |= ((unsigned long)type) << 28;
    response |= ((unsigned long)id) << 16;
    if (um_otrm_parity(response))
    {
        response |= (1ul << 31);
    }
    return response;
}

bool um_otrm_is_valid_response(unsigned long response)
{
    if (um_otrm_parity(response))
        return false;
    uint8_t msg_type = (response << 1) >> 29;
    return msg_type == UM_OTRM_MSG_READ_ACK || msg_type == UM_OTRM_MSG_WRITE_ACK;
}

bool um_otrm_is_valid_request(unsigned long request)
{
    if (um_otrm_parity(request))
        return false;
    uint8_t msg_type = (request << 1) >> 29;
    return msg_type == UM_OTRM_MSG_READ_DATA || msg_type == UM_OTRM_MSG_WRITE_DATA;
}

const char *um_otrm_response_status_to_string(um_otrm_response_status_t status)
{
    switch (status)
    {
    case UM_OTRM_RESPONSE_NONE:
        return "NONE";
    case UM_OTRM_RESPONSE_SUCCESS:
        return "SUCCESS";
    case UM_OTRM_RESPONSE_INVALID:
        return "INVALID";
    case UM_OTRM_RESPONSE_TIMEOUT:
        return "TIMEOUT";
    default:
        return "UNKNOWN";
    }
}

const char *um_otrm_message_type_to_string(um_otrm_message_type_t type)
{
    switch (type)
    {
    case UM_OTRM_MSG_READ_DATA:
        return "READ_DATA";
    case UM_OTRM_MSG_WRITE_DATA:
        return "WRITE_DATA";
    case UM_OTRM_MSG_INVALID_DATA:
        return "INVALID_DATA";
    case UM_OTRM_MSG_RESERVED:
        return "RESERVED";
    case UM_OTRM_MSG_READ_ACK:
        return "READ_ACK";
    case UM_OTRM_MSG_WRITE_ACK:
        return "WRITE_ACK";
    case UM_OTRM_MSG_DATA_INVALID:
        return "DATA_INVALID";
    case UM_OTRM_MSG_UNKNOWN_DATA_ID:
        return "UNKNOWN_DATA_ID";
    default:
        return "UNKNOWN";
    }
}

/* Request builders */

unsigned long um_otrm_build_set_boiler_status_request(bool enable_central_heating, bool enable_hot_water,
                                                      bool enable_cooling, bool enable_outside_temp_comp,
                                                      bool enable_central_heating2)
{
    unsigned int data = enable_central_heating | (enable_hot_water << 1) | (enable_cooling << 2) |
                        (enable_outside_temp_comp << 3) | (enable_central_heating2 << 4);
    data <<= 8;
    return um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_STATUS, data);
}

unsigned long um_otrm_build_set_boiler_temperature_request(float temperature)
{
    unsigned int data = um_otrm_temperature_to_data(temperature);
    return um_otrm_build_request(UM_OTRM_MSG_WRITE_DATA, UM_OTRM_ID_TSET, data);
}

unsigned long um_otrm_build_get_boiler_temperature_request(void)
{
    return um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_TBOILER, 0);
}

/* Response parsers */

bool um_otrm_is_fault(unsigned long response)
{
    return response & 0x1;
}

bool um_otrm_is_central_heating_active(unsigned long response)
{
    return response & 0x2;
}

bool um_otrm_is_hot_water_active(unsigned long response)
{
    return response & 0x4;
}

bool um_otrm_is_flame_on(unsigned long response)
{
    return response & 0x8;
}

bool um_otrm_is_cooling_active(unsigned long response)
{
    return response & 0x10;
}

bool um_otrm_is_diagnostic(unsigned long response)
{
    return response & 0x40;
}

uint16_t um_otrm_get_uint(unsigned long response)
{
    return (uint16_t)(response & 0xffff);
}

float um_otrm_get_float(unsigned long response)
{
    uint16_t u88 = um_otrm_get_uint(response);
    return (u88 & 0x8000) ? -(0x10000L - u88) / 256.0f : u88 / 256.0f;
}

unsigned int um_otrm_temperature_to_data(float temperature)
{
    if (temperature < 0)
        temperature = 0;
    if (temperature > 100)
        temperature = 100;
    return (unsigned int)(temperature * 256);
}

/* Basic requests */

unsigned long um_otrm_set_boiler_status(um_otrm_t *ot, bool enable_central_heating, bool enable_hot_water,
                                        bool enable_cooling, bool enable_outside_temp_comp,
                                        bool enable_central_heating2)
{
    return um_otrm_send_request(ot, um_otrm_build_set_boiler_status_request(enable_central_heating, enable_hot_water,
                                                                            enable_cooling, enable_outside_temp_comp,
                                                                            enable_central_heating2));
}

bool um_otrm_set_boiler_temperature(um_otrm_t *ot, float temperature)
{
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_set_boiler_temperature_request(temperature));
    return um_otrm_is_valid_response(response);
}

float um_otrm_get_boiler_temperature(um_otrm_t *ot)
{
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_get_boiler_temperature_request());
    return um_otrm_is_valid_response(response) ? um_otrm_get_float(response) : 0;
}

float um_otrm_get_return_temperature(um_otrm_t *ot)
{
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_TRET, 0));
    return um_otrm_is_valid_response(response) ? um_otrm_get_float(response) : 0;
}

bool um_otrm_set_dhw_setpoint(um_otrm_t *ot, float temperature)
{
    unsigned int data = um_otrm_temperature_to_data(temperature);
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_request(UM_OTRM_MSG_WRITE_DATA, UM_OTRM_ID_TDHW_SET, data));
    return um_otrm_is_valid_response(response);
}

float um_otrm_get_dhw_temperature(um_otrm_t *ot)
{
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_TDHW, 0));
    return um_otrm_is_valid_response(response) ? um_otrm_get_float(response) : 0;
}

float um_otrm_get_modulation(um_otrm_t *ot)
{
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_REL_MOD_LEVEL, 0));
    return um_otrm_is_valid_response(response) ? um_otrm_get_float(response) : 0;
}

float um_otrm_get_pressure(um_otrm_t *ot)
{
    unsigned long response = um_otrm_send_request(ot, um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_CH_PRESSURE, 0));
    return um_otrm_is_valid_response(response) ? um_otrm_get_float(response) : 0;
}

unsigned char um_otrm_get_fault(um_otrm_t *ot)
{
    return ((um_otrm_send_request(ot, um_otrm_build_request(UM_OTRM_MSG_READ_DATA, UM_OTRM_ID_ASF_FLAGS, 0)) >> 8) & 0xff);
}