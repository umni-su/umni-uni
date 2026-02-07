/**
 * @file um_dio.c
 * @brief Digital Input/Output module implementation for PCF8574
 * @version 1.0.0
 */

#include "um_dio.h"
#include "um_nvs.h"
#include "pcf8574.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "string.h"
#include "base_config.h"
#include "i2cdev.h"

static const char *TAG = "um_dio";

/* I2C device descriptors */
static i2c_dev_t pcf8574_output_dev;
static i2c_dev_t pcf8574_input_dev;

/* Current port states */
static uint8_t output_data = 0xFF; // Default all outputs high (inactive)
static uint8_t input_data = 0xFF;  // Current input states

/* Interrupt handling */
static TaskHandle_t input_task_handle = NULL;
static QueueHandle_t input_queue = NULL;

/* Configuration constants */
#define I2C_OUTPUT_ADDR 0x27
#define I2C_INPUT_ADDR  0x26
#define INT_PIN         CONFIG_UM_CFG_PCF_INT

/* Input mapping from config index to port index */
static const uint8_t input_index_map[] = {
    0,  // Not used (index 0)
    CONFIG_UM_CFG_INP1_INDEX,
    CONFIG_UM_CFG_INP2_INDEX,
    CONFIG_UM_CFG_INP3_INDEX,
    CONFIG_UM_CFG_INP4_INDEX,
    CONFIG_UM_CFG_INP5_INDEX,
    CONFIG_UM_CFG_INP6_INDEX,
};

/* Output mapping from config index to port index */
static const uint8_t output_index_map[] = {
    0,  // Not used (index 0)
    CONFIG_UM_CFG_OUT1_INDEX,
    CONFIG_UM_CFG_OUT2_INDEX,
    CONFIG_UM_CFG_OUT3_INDEX,
    CONFIG_UM_CFG_OUT4_INDEX,
    CONFIG_UM_CFG_OUT5_INDEX,
    CONFIG_UM_CFG_OUT6_INDEX,
    CONFIG_UM_CFG_OUT7_INDEX,
    CONFIG_UM_CFG_OUT8_INDEX,
};

/* IRAM interrupt handler */
static void IRAM_ATTR pcf8574_interrupt_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t dummy = 0;
    
    /* Send notification to task */
    if (input_queue != NULL) {
        xQueueSendFromISR(input_queue, &dummy, &xHigherPriorityTaskWoken);
    }
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/* Input monitoring task */
static void input_monitor_task(void *arg)
{
    uint8_t dummy;
    uint8_t new_state;
    
    while (1) {
        /* Wait for interrupt notification */
        if (xQueueReceive(input_queue, &dummy, portMAX_DELAY)) {
            /* Read current input states */
            if (pcf8574_port_read(&pcf8574_input_dev, &new_state) == ESP_OK) {
                /* Check for changes */
                if (new_state != input_data) {
                    uint8_t changed = input_data ^ new_state;

                    ESP_LOGI(TAG, "!!!Input changed");
                    
                    /* Log changes for each input */
                    for (uint8_t i = 1; i <= 6; i++) {
#if UM_FEATURE_ENABLED(INPUTS)
                        if (changed & (1 << (input_index_map[i] - 1))) {
                            bool old_state = (input_data >> (input_index_map[i] - 1)) & 0x01;
                            bool new_bit = (new_state >> (input_index_map[i] - 1)) & 0x01;
                            
                            ESP_LOGI(TAG, "Input %d changed: %d -> %d", 
                                     i, old_state, new_bit);
                        }
#endif
                    }
                    
                    /* Update stored state */
                    input_data = new_state;
                }
            }
        }
    }
}

/* Initialize output PCF8574 */
static esp_err_t init_output_pcf8574(void)
{
    esp_err_t res;
    
#if UM_FEATURE_ENABLED(OUTPUTS)
    ESP_LOGI(TAG, "Initializing output PCF8574 at address 0x%02X", I2C_OUTPUT_ADDR);
    
    /* Сначала инициализируем I2C дескриптор */
    memset(&pcf8574_output_dev, 0, sizeof(i2c_dev_t));
    pcf8574_output_dev.cfg.master.clk_speed = 5000;
    
    /* Инициализируем I2C */
    res = pcf8574_init_desc(&pcf8574_output_dev, I2C_OUTPUT_ADDR, 0,
                           CONFIG_I2C_MASTER_SDA_GPIO,
                           CONFIG_I2C_MASTER_SCL_GPIO);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize output PCF8574: 0x%X", res);
        return res;
    }
    
    /* Только ПОСЛЕ успешной инициализации загружаем состояние */
    if(um_nvs_get_outputs_data(&output_data) == ESP_OK){
        ESP_LOGI(TAG, "Loaded output states: 0x%02X", output_data);
    } else {
        output_data = 0xFF;
    }
    
    
    /* Записываем начальное состояние */
    res = pcf8574_port_write(&pcf8574_output_dev, output_data);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write initial output state: 0x%X", res);
        return res;
    }
    
    ESP_LOGI(TAG, "Output PCF8574 initialized successfully");
#endif
    
    return ESP_OK;
}

/* Initialize input PCF8574 with interrupt */
static esp_err_t init_input_pcf8574(void)
{
    esp_err_t res;
    
#if UM_FEATURE_ENABLED(INPUTS)
    ESP_LOGI(TAG, "Initializing input PCF8574 at address 0x%02X", I2C_INPUT_ADDR);
    
    /* Initialize device descriptor */
    memset(&pcf8574_input_dev, 0, sizeof(i2c_dev_t));
    pcf8574_input_dev.cfg.master.clk_speed = 5000;
    
    /* Initialize I2C descriptor */
    res = pcf8574_init_desc(&pcf8574_input_dev, I2C_INPUT_ADDR, 0,
                           CONFIG_I2C_MASTER_SDA_GPIO,
                           CONFIG_I2C_MASTER_SCL_GPIO);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize input PCF8574: 0x%X", res);
        return res;
    }
    
    /* Set all input pins high (pull-up enabled) */
    res = pcf8574_port_write(&pcf8574_input_dev, 0xFF);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure input pins: 0x%X", res);
        return res;
    }
    
    /* Read initial input states */
    res = pcf8574_port_read(&pcf8574_input_dev, &input_data);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read initial input states: 0x%X", res);
        return res;
    }
    
    ESP_LOGI(TAG, "Initial input states: 0x%02X", input_data);
    
    /* Configure interrupt pin */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_FLOATING,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);
    
    /* Install interrupt handler */
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
    }
    gpio_isr_handler_add(INT_PIN, pcf8574_interrupt_handler, NULL);
    
    /* Create queue for interrupt notifications */
    input_queue = xQueueCreate(10, sizeof(uint8_t));
    if (input_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create input queue");
        return ESP_ERR_NO_MEM;
    }
    
    /* Create input monitoring task */
    BaseType_t task_res = xTaskCreate(input_monitor_task,
                                     "dio_input_monitor",
                                     4096,
                                     NULL,
                                     2,
                                     &input_task_handle);
    if (task_res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create input monitoring task");
        vQueueDelete(input_queue);
        input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Input PCF8574 initialized with interrupt on GPIO %d", INT_PIN);
#endif
    
    return ESP_OK;
}

/* Map channel index to port bit position */
static uint8_t get_output_bit_position(uint8_t output_idx)
{
    if (output_idx >= sizeof(output_index_map)) {
        return 0;
    }
    return output_index_map[output_idx] - 1; // Convert to 0-based bit position
}

static uint8_t get_input_bit_position(uint8_t input_idx)
{
    if (input_idx >= sizeof(input_index_map)) {
        return 0;
    }
    return input_index_map[input_idx] - 1; // Convert to 0-based bit position
}

/* Добавьте функцию инициализации i2cdev: */
static esp_err_t init_i2cdev(void)
{
    esp_err_t res;
    
    /* Инициализируем i2cdev */
    res = i2cdev_init();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize i2cdev: 0x%X", res);
        return res;
    }
    
    ESP_LOGI(TAG, "i2cdev initialized successfully");
    return ESP_OK;
}

/* Public API implementation */

esp_err_t um_dio_init(void)
{
    esp_err_t res;
    
    ESP_LOGI(TAG, "Initializing DIO module");

    res = init_i2cdev();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "i2cdev initialization failed");
        return res;
    }
    
    /* Initialize outputs */
    res = init_output_pcf8574();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Output initialization failed");
        return res;
    }
    
    /* Initialize inputs */
    res = init_input_pcf8574();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Input initialization failed");
        return res;
    }
    
    ESP_LOGI(TAG, "DIO module initialized successfully");
    return ESP_OK;
}

esp_err_t um_dio_get_input(uint8_t input_idx, bool *state)
{
#if UM_FEATURE_ENABLED(INPUTS)
    if (input_idx < 1 || input_idx > 6) {
        ESP_LOGE(TAG, "Invalid input index: %d", input_idx);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Check if specific input is enabled */
    switch (input_idx) {
#if UM_FEATURE_ENABLED(INP1)
        case 1: break;
#endif
#if UM_FEATURE_ENABLED(INP2)
        case 2: break;
#endif
#if UM_FEATURE_ENABLED(INP3)
        case 3: break;
#endif
#if UM_FEATURE_ENABLED(INP4)
        case 4: break;
#endif
#if UM_FEATURE_ENABLED(INP5)
        case 5: break;
#endif
#if UM_FEATURE_ENABLED(INP6)
        case 6: break;
#endif
        default:
            ESP_LOGE(TAG, "Input %d not enabled", input_idx);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    uint8_t bit_pos = get_input_bit_position(input_idx);
    *state = (input_data >> bit_pos) & 0x01;
    
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Inputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t um_dio_get_all_inputs(uint8_t *states)
{
#if UM_FEATURE_ENABLED(INPUTS)
    *states = input_data;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Inputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static uint8_t get_output_index(um_do_port_index_t output_idx)
{
    switch(output_idx) {
        case 0: return CONFIG_UM_CFG_OUT1_INDEX;
        case 1: return CONFIG_UM_CFG_OUT2_INDEX;
        case 2: return CONFIG_UM_CFG_OUT3_INDEX;
        case 3: return CONFIG_UM_CFG_OUT4_INDEX;
        case 4: return CONFIG_UM_CFG_OUT5_INDEX;
        case 5: return CONFIG_UM_CFG_OUT6_INDEX;
        case 6: return CONFIG_UM_CFG_OUT7_INDEX;
        case 7: return CONFIG_UM_CFG_OUT8_INDEX;
        default: return 0;
    }
}

esp_err_t um_dio_set_output(um_do_port_index_t output_idx, um_do_level_t level)
{
#if UM_FEATURE_ENABLED(OUTPUTS)
    if (output_idx < 0 || output_idx > 7) {
        ESP_LOGE(TAG, "Invalid output index: %d", output_idx);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Check if specific output is enabled */
    switch (output_idx) {
#if UM_FEATURE_ENABLED(OUT1)
        case 0: break;
#endif
#if UM_FEATURE_ENABLED(OUT2)
        case 1: break;
#endif
#if UM_FEATURE_ENABLED(OUT3)
        case 2: break;
#endif
#if UM_FEATURE_ENABLED(OUT4)
        case 3: break;
#endif
#if UM_FEATURE_ENABLED(OUT5)
        case 4: break;
#endif
#if UM_FEATURE_ENABLED(OUT6)
        case 5: break;
#endif
#if UM_FEATURE_ENABLED(OUT7)
        case 6: break;
#endif
#if UM_FEATURE_ENABLED(OUT8)
        case 7: break;
#endif
        default:
            ESP_LOGE(TAG, "Output %d not enabled", output_idx);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    //uint8_t bit_pos = get_output_bit_position(output_idx);
    
    if (level == DO_LOW)
    {
        output_data = output_data | (1 << get_output_index(output_idx));
    }
    else
    {
        output_data = output_data & ~(1 << get_output_index(output_idx));
    }
    
    /* Write to PCF8574 */
    esp_err_t res = pcf8574_port_write(&pcf8574_output_dev, output_data);
    if (res == ESP_OK) {
        /* Save to NVS */
        res = um_nvs_set_outputs_data(output_data);
    }
    
    return res;
#else
    ESP_LOGE(TAG, "Outputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t um_dio_get_output(uint8_t output_idx, bool *state)
{
#if UM_FEATURE_ENABLED(OUTPUTS)
    if (output_idx < 1 || output_idx > 8) {
        ESP_LOGE(TAG, "Invalid output index: %d", output_idx);
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t bit_pos = get_output_bit_position(output_idx);
    *state = (output_data >> bit_pos) & 0x01;
    
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Outputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t um_dio_set_all_outputs(uint8_t states)
{
#if UM_FEATURE_ENABLED(OUTPUTS)
    output_data = states;
    
    /* Write to PCF8574 */
    esp_err_t res = pcf8574_port_write(&pcf8574_output_dev, output_data);
    if (res == ESP_OK) {
        /* Save to NVS */
        res = um_nvs_set_outputs_data(output_data);
    }
    
    return res;
#else
    ESP_LOGE(TAG, "Outputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t um_dio_get_all_outputs(uint8_t *states)
{
#if UM_FEATURE_ENABLED(OUTPUTS)
    *states = output_data;
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Outputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t um_dio_toggle_output(uint8_t output_idx)
{
#if UM_FEATURE_ENABLED(OUTPUTS)
    bool current_state;
    esp_err_t res = um_dio_get_output(output_idx, &current_state);
    if (res != ESP_OK) {
        return res;
    }
    
    return um_dio_set_output(output_idx, !current_state);
#else
    ESP_LOGE(TAG, "Outputs feature not enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t um_dio_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing DIO module");
    
    /* Clean up input task and queue */
    if (input_task_handle != NULL) {
        vTaskDelete(input_task_handle);
        input_task_handle = NULL;
    }
    
    if (input_queue != NULL) {
        vQueueDelete(input_queue);
        input_queue = NULL;
    }
    
    /* Remove interrupt handler */
    gpio_isr_handler_remove(CONFIG_UM_CFG_PCF_INT);
    gpio_uninstall_isr_service();
    
    /* Free I2C descriptors */
    pcf8574_free_desc(&pcf8574_output_dev);
    pcf8574_free_desc(&pcf8574_input_dev);

    i2cdev_done();
    
    ESP_LOGI(TAG, "DIO module deinitialized");
    return ESP_OK;
}