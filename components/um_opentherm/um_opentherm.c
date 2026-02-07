#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"

#include "base_config.h"
#include "um_nvs.h"
#include "um_events.h"
#include "um_opentherm.h"

//CONFIG_UM_CFG_OT_IN_GPIO=26
//CONFIG_UM_CFG_OT_OUT_GPIO=25

#if UM_FEATURE_ENABLED(OPENTHERM)

#define ESP_INTR_FLAG_DEFAULT 0

static uint8_t targetDHWTemp = 59;
static uint8_t targetCHTemp = 60;
static bool otEnabled = true; // OT global status
static bool needReset = false;

bool enableCentralHeating = true;
bool enableHotWater = true;
bool enableCooling = false;
bool enableOutsideTemperatureCompensation = false;
bool enableCentralHeating2 = false;

static const char *TAG = "opentherm";

static um_ot_data_t ot_data;

TaskHandle_t ot_handle = NULL;

open_therm_response_status_t ot_response_status;

bool is_busy = false;

bool initialized = false;

unsigned long status;

static unsigned char task_count = 0;
static unsigned char task_count_max_to_send_data = 120; // 120 sec delay counter to send mqtt message

static bool need_read_pump = false;

void um_opentherm_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id != UMNI_EVENT_OPENTHERM_CH_ON && id != UMNI_EVENT_OPENTHERM_CH_OFF)
    {
        return;
    }
    bool otch = id == UMNI_EVENT_OPENTHERM_CH_ON ? true : false;

    um_ot_update_state(otch, ot_data.otdhwsp, ot_data.ottbsp);

    // um_ot_set_boiler_status(
    //     enableCentralHeating,
    //     enableHotWater, enableCooling,
    //     enableOutsideTemperatureCompensation,
    //     enableCentralHeating2);
    ESP_LOGI(TAG, "OT CH triggered by event. OT is %s", otch ? "ON" : "OFF");
}

void um_opentherm_control_task_handler(void *pvParameter)
{
    // Устанавливаем начальные целевые значения из NVS
    um_nvs_get_ot_enabled(&otEnabled);
    um_nvs_get_ot_dhw_setpoint(&targetDHWTemp);
    um_nvs_get_ot_ch_setpoint(&targetCHTemp);
    um_nvs_get_ot_ch_enabled(&enableCentralHeating);
    um_nvs_get_ot_dhw_enabled(&enableHotWater);
    um_nvs_get_ot_outdoor_temp_comp(&enableOutsideTemperatureCompensation);

    // Переменные для периодической проверки
    static bool last_enabled_state = false;
    static TickType_t last_state_check_time = 0;
    static TickType_t last_nvs_check_time = 0;
    
    // Первая инициализация
    last_enabled_state = otEnabled;
    last_state_check_time = xTaskGetTickCount();
    last_nvs_check_time = xTaskGetTickCount();

    while (true)
    {
        TickType_t loop_start_time = xTaskGetTickCount();
        
        // Если OpenTherm отключен
        if(!otEnabled)
        {
            // Логируем изменение состояния (один раз при переходе)
            if (last_enabled_state != otEnabled)
            {
                ESP_LOGI(TAG, "OpenTherm disabled, entering low-power mode");
                last_enabled_state = otEnabled;
            }
            
            // Основная точка блокировки - даём CPU поработать другим задачам
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            // Периодическая проверка NVS на изменение состояния (раз в 5 секунд)
            TickType_t current_time = xTaskGetTickCount();
            if ((current_time - last_nvs_check_time) > pdMS_TO_TICKS(5000))
            {
                um_nvs_get_ot_enabled(&otEnabled);
                last_nvs_check_time = current_time;
                
                if (otEnabled)
                {
                    ESP_LOGI(TAG, "OpenTherm enabled from NVS, initializing...");
                    // Обновляем параметры при включении
                    um_nvs_get_ot_dhw_setpoint(&targetDHWTemp);
                    um_nvs_get_ot_ch_setpoint(&targetCHTemp);
                    um_nvs_get_ot_ch_enabled(&enableCentralHeating);
                    um_nvs_get_ot_dhw_enabled(&enableHotWater);
                    um_nvs_get_ot_outdoor_temp_comp(&enableOutsideTemperatureCompensation);
                }
            }
            
            continue; // Переход к следующей итерации цикла
        }
        
        // Если OpenTherm включен - обычная логика работы
        bool stat = false;
        esp_err_t res = um_ot_set_boiler_status(
            enableCentralHeating,
            enableHotWater, enableCooling,
            enableOutsideTemperatureCompensation,
            enableCentralHeating2);
        um_ot_set_boiler_temp(targetCHTemp);
        um_ot_set_dhw_setpoint(targetDHWTemp);

        if (res != ESP_OK)
        {
            ot_data.adapter_success = false;
            ESP_LOGD(TAG, "Opentherm um_ot_set_boiler_status return ESP_FAIL, retry...");
            res = um_ot_set_boiler_status(
                enableCentralHeating,
                enableHotWater, enableCooling,
                enableOutsideTemperatureCompensation,
                enableCentralHeating2);
            um_ot_set_boiler_temp(targetCHTemp);
            um_ot_set_dhw_setpoint(targetDHWTemp);
            
            // ВАЖНО: Задержка перед повторной попыткой
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        } 
        else 
        {
            ESP_LOGI(TAG, "Set CH: %i,  DHW %i", targetCHTemp, targetDHWTemp);
        }

        if (!is_busy)
        {
            is_busy = true;

            ESP_LOGI(TAG, "\r\n====== OPENTHERM DATA =====");
            ESP_LOGI(TAG, "Free heap size before: %ld", esp_get_free_heap_size());
            ESP_LOGI(TAG, "NVS OT values - chen: %d, hwa: %d, dhwspt: %d chspt: %d", 
                     enableCentralHeating, enableHotWater, targetDHWTemp, targetCHTemp);

            ot_response_status = esp_ot_get_last_response_status();

            if (ot_response_status == OT_STATUS_SUCCESS)
            {
                // установка модуляции
                // stat = esp_ot_set_modulation_level(ot_data.mod);
                // if (!stat)
                // {
                //     ESP_LOGE(TAG, "Error set modulation level");
                // }
                // else
                // {
                //     ESP_LOGI(TAG, "Modulation level set success to %d", ot_data.mod);
                // }
                // установка кривой нагрева
                if (enableOutsideTemperatureCompensation)
                {
                    // if (ot_data.othcr > 0 && ot_data.othcr < 100)
                    // {
                    //     stat = esp_ot_set_otc_curve_ratio(ot_data.othcr);
                    //     if (!stat)
                    //     {
                    //         ESP_LOGE(TAG, "Error set heat curve ratio");
                    //     }
                    //     else
                    //     {
                    //         ESP_LOGE(TAG, "Heat curve ratio set success");
                    //     }
                    // }
                }

                ot_data.ready = false;

                esp_ot_slave_config_t slave = esp_ot_get_slave_configuration();
                ot_data.slave_config = slave;

                const float slave_ot_version = esp_ot_get_slave_ot_version();
                const unsigned long slave_product_version = esp_ot_get_slave_product_version();
                ESP_LOGI(TAG, "Slave OT Version: %.1f", slave_ot_version);
                ESP_LOGI(TAG, "Slave Version: %08lX", slave_product_version);

                ot_data.slave_ot_version = slave_ot_version;
                ot_data.slave_product_version = slave_product_version;

                const float modulation = esp_ot_get_modulation();
                const float pressure = esp_ot_get_pressure();
                const float dhwTemp = esp_ot_get_dhw_temperature();
                const float chTemp = esp_ot_get_boiler_temperature();
                const float retTemp = esp_ot_get_return_temperature();

                ot_data.status = ot_response_status;
                ot_data.otch = enableCentralHeating;
                ot_data.ototc = enableOutsideTemperatureCompensation;
                ot_data.pressure = pressure;
                ot_data.modulation = modulation;
                ot_data.dhw_temperature = dhwTemp;
                ot_data.boiler_temperature = chTemp;
                ot_data.return_temperature = retTemp;
                ot_data.otdhwsp = targetDHWTemp;
                ot_data.ottbsp = targetCHTemp;
                ot_data.dhw_setpoint = esp_ot_get_dhw_setpoint();
                ot_data.flow_rate = esp_ot_get_ch2_flow();

                ESP_LOGI(TAG, "Central Heating: %s", ot_data.central_heating_active ? "ON" : "OFF");
                ESP_LOGI(TAG, "DHW setpoint: %.1f", ot_data.dhw_setpoint);
                ESP_LOGI(TAG, "Hot Water: %s", ot_data.hot_water_active ? "ON" : "OFF");
                ESP_LOGI(TAG, "Flame: %s", ot_data.flame_on ? "ON" : "OFF");
                ESP_LOGI(TAG, "Fault: %s", ot_data.is_fault ? "YES" : "NO");
                ESP_LOGI(TAG, "OTC: %s", ot_data.ototc ? "ON" : "OFF");
                if (ot_data.is_fault)
                {

                    esp_ot_asf_flags_t flags = esp_ot_get_asf_flags();
                    ot_data.asf_flags = flags;
                    ESP_LOGE(TAG, "FAULT CODE: %d, DIAG CODE: %d", flags.fault_code, flags.diag_code);
                    ESP_LOGE(TAG, "Is service: %d", flags.is_service_request);
                    ESP_LOGE(TAG, "Can reset: %d", flags.can_reset);
                    ESP_LOGE(TAG, "Is pressure error: %d", flags.is_air_press_fault);
                    ESP_LOGE(TAG, "Is gas error: %d", flags.is_gas_flame_fault);
                    ESP_LOGE(TAG, "Is low water pres: %d", flags.is_low_water_press);
                    ESP_LOGE(TAG, "Is water over temp: %d", flags.is_water_over_temp);

                    if (needReset)
                    {
                        ot_reset();
                        needReset = false;
                        ESP_LOGE(TAG, "Try to reset error code...");
                    }
                }
                ESP_LOGI(TAG, "Tret: %.1f", dhwTemp);
                ESP_LOGI(TAG, "CH Temp: %.1f", chTemp);
                ESP_LOGI(TAG, "Pressure: %.1f", pressure);
                ESP_LOGI(TAG, "Modulation:%.1f", modulation);

                ot_data.flow_rate = esp_ot_get_flow_rate();
                ESP_LOGI(TAG, "esp_ot_get_flow_rate: %.1f", ot_data.flow_rate);
                vTaskDelay(pdMS_TO_TICKS(1));

                ot_data.ch_max_setpoint = esp_ot_get_ch_max_setpoint();
                ESP_LOGI(TAG, "esp_ot_get_ch_max_setpoint: %.1f", ot_data.ch_max_setpoint);
                vTaskDelay(pdMS_TO_TICKS(1));

                ot_data.outside_temperature = esp_ot_get_outside_temperature();
                ESP_LOGI(TAG, "esp_ot_get_outside_temperature: %.1f", ot_data.outside_temperature);
                vTaskDelay(pdMS_TO_TICKS(1));

                esp_ot_min_max_t dhw_bounds = esp_ot_get_dhw_upper_lower_bounds();
                ot_data.dhw_min_max = dhw_bounds;
                ESP_LOGI(TAG, "dhw_bounds min: %d, max: %d", dhw_bounds.min, dhw_bounds.max);
                vTaskDelay(pdMS_TO_TICKS(1));

                esp_ot_min_max_t ch_bounds = esp_ot_get_ch_upper_lower_bounds();
                ot_data.ch_min_max = ch_bounds;
                ESP_LOGI(TAG, "ch_bounds min: %d, max: %d", ot_data.ch_min_max.min, ot_data.ch_min_max.max);
                vTaskDelay(pdMS_TO_TICKS(1));

                esp_ot_cap_mod_t cap_mod = esp_ot_get_max_capacity_min_modulation();
                ot_data.cap_mod = cap_mod;
                ESP_LOGI(TAG, "ch_bounds cap: %d kw, min_mod: %d", ot_data.cap_mod.kw, ot_data.cap_mod.min_modulation);
                vTaskDelay(pdMS_TO_TICKS(1));

                // кривая нагрева - верхние и нижние границы
                ot_data.curve_bounds = esp_ot_get_heat_curve_ul_bounds();
                ESP_LOGI(TAG, "curve_bounds min: %d, max: %d", ot_data.curve_bounds.min, ot_data.curve_bounds.max);
                vTaskDelay(pdMS_TO_TICKS(1));
                // чтение значения кривой с котла
                // ot_data.heat_curve_ratio = esp_ot_get_heat_curve_ratio();
                // ESP_LOGI(TAG, "heat_curve_ratio: %.1f", ot_data.heat_curve_ratio);

                // if (ot_data.slave_config.pump_control_allowed && need_read_pump)
                // {
                //     uint16_t val = esp_ot_read_dhw_pump_starts();
                //     ESP_LOGI(TAG, "dhw_pump_starts : %d", val);

                //     val = esp_ot_read_dhw_pump_hours();
                //     ESP_LOGI(TAG, "dhw_pump_hours : %d", val);

                //     val = esp_ot_read_ch_pump_starts();
                //     ESP_LOGI(TAG, "ch_pump_starts : %d", val);

                //     val = esp_ot_read_ch_pump_hours();
                //     ESP_LOGI(TAG, "ch_pump_hours : %d", val);
                // }
                
                ot_data.adapter_success = true;
                ot_data.ready = true;
            }
            else
            {
                ESP_LOGW(TAG, "Error reading %d", ot_response_status);
            }
            
            ESP_LOGI(TAG, "Free heap size after: %ld", esp_get_free_heap_size());
            ESP_LOGI(TAG, "====== OPENTHERM =====\r\n\r\n");
            is_busy = false;
        }
        
        // Обновление счетчика и отправка данных
        if (task_count >= task_count_max_to_send_data)
        {
            // send mqtt
            task_count = 0;
            um_event_publish(UMNI_EVENT_OPENTHERM_SET_DATA, NULL, sizeof(NULL), portMAX_DELAY);
        }
        else
        {
            task_count++;
        }
        
        // Обеспечиваем минимальный период цикла (1 секунда)
        TickType_t loop_elapsed_time = xTaskGetTickCount() - loop_start_time;
        if (loop_elapsed_time < pdMS_TO_TICKS(1000))
        {
            vTaskDelay(pdMS_TO_TICKS(1000) - loop_elapsed_time);
        }
        else
        {
            // Если цикл занял больше 1 секунды, даём минимальную паузу
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // Периодическая проверка NVS на изменение состояния (только в активном режиме)
        TickType_t current_time = xTaskGetTickCount();
        if ((current_time - last_nvs_check_time) > pdMS_TO_TICKS(5000))
        {
            bool new_ot_enabled = false;
            um_nvs_get_ot_enabled(&new_ot_enabled);
            if (new_ot_enabled != otEnabled)
            {
                otEnabled = new_ot_enabled;
                last_nvs_check_time = current_time;
                
                if (!otEnabled)
                {
                    ESP_LOGI(TAG, "OpenTherm disabled from NVS, stopping operations");
                }
            }
            last_nvs_check_time = current_time;
        }
    }
    
    vTaskDelete(NULL);
}

esp_err_t um_ot_set_boiler_status(
    bool enable_central_heating,
    bool enable_hot_water,
    bool enable_cooling,
    bool enable_outside_temperature_compensation,
    bool enable_central_heating2)
{
    // vTaskSuspend(ot_handle);

    enableCentralHeating = enable_central_heating;
    enableHotWater = enable_hot_water;
    enableCooling = enable_cooling;
    enableOutsideTemperatureCompensation = enable_outside_temperature_compensation;
    enableCentralHeating2 = enable_central_heating2;

    ot_data.otch = enableCentralHeating;
    
    esp_err_t res = ESP_OK;
    status = esp_ot_set_boiler_status(
        enableCentralHeating,
        enableHotWater,
        enableCooling,
        enableOutsideTemperatureCompensation,
        enableCentralHeating2);

    ot_response_status = esp_ot_get_last_response_status();
    if (ot_response_status == OT_STATUS_SUCCESS)
    {
        ot_data.central_heating_active = esp_ot_is_central_heating_active(status);
        ot_data.hot_water_active = esp_ot_is_hot_water_active(status);
        ot_data.flame_on = esp_ot_is_flame_on(status);
        ot_data.is_fault = esp_ot_is_fault(status);
        ESP_LOGW(TAG, "[um_ot_set_boiler_status] enableCentralHeating: %d  chtemp: %d dhwtemp:%d", enableCentralHeating, targetCHTemp, targetDHWTemp);
    }
    else if (ot_response_status == OT_STATUS_TIMEOUT)
    {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "OT Communication Timeout");
    }
    else if (ot_response_status == OT_STATUS_INVALID)
    {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "OT Communication Invalid");
    }
    else if (ot_response_status == OT_STATUS_NONE)
    {
        res = ESP_FAIL;
        ESP_LOGE(TAG, "OpenTherm not initialized");
    }
    else
    {
        res = ESP_FAIL;
    }

    // vTaskResume(ot_handle);

    return res;
}

// двухконтурный котел
void um_ot_set_ch2(bool active)
{
    if (!ot_data.slave_config.ch2_present)
    {
        enableCentralHeating2 = active;
        um_nvs_write_i8(UM_NVS_KEY_OT_CH2, active ? 1 : 0);
    }
}

// установка компенсации по температурному датчику (наружней т-ры)
void um_ot_set_outside_temp_comp(bool state)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_OTC, state ? 1 : 0);
    enableOutsideTemperatureCompensation = state;
}

void um_ot_set_modulation_level(int level)
{
    if (level < 0 || level > 99)
        level = 99;
    um_nvs_write_i8(UM_NVS_KEY_OT_MOD, level);
    ot_data.mod = level;
}
// активация горячего водостнабжения
void um_ot_set_hot_water_active(bool hwa)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_DHW, hwa ? 1 : 0);
    ot_data.hwa = hwa;
    enableHotWater = hwa;
}

// Установка целевой температуры бойлера
void um_ot_set_boiler_temp(float temp)
{
    vTaskDelay(10 / portTICK_PERIOD_MS);
    ot_response_status = esp_ot_get_last_response_status();
    if (ot_response_status == OT_STATUS_SUCCESS)
    {
        targetCHTemp = temp;
        um_nvs_write_i8(UM_NVS_KEY_OT_CH_SETPOINT, targetCHTemp);
        esp_ot_set_boiler_temperature(targetCHTemp);
        ESP_LOGI(TAG, "Set CH Temp to: %i", targetCHTemp);
    }
}
// Установка целевой горячей воды (ГВС)
void um_ot_set_dhw_setpoint(float temp)
{
    vTaskDelay(10 / portTICK_PERIOD_MS);
    ot_response_status = esp_ot_get_last_response_status();
    if (ot_response_status == OT_STATUS_SUCCESS)
    {
        targetDHWTemp = temp;
        um_nvs_write_i8(UM_NVS_KEY_OT_DHW_SETPOINT, temp);
        esp_ot_set_dhw_setpoint(targetDHWTemp);
        ESP_LOGI(TAG, "Set DHW Temp to: %i", targetDHWTemp);
    }
}

void um_ot_init()
{
    um_event_subscribe(UMNI_EVENT_ANY, um_opentherm_event_handler, NULL);

    esp_ot_init(
        CONFIG_UM_CFG_OT_IN_GPIO,
        CONFIG_UM_CFG_OT_OUT_GPIO,
        false,
        NULL);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Get or set DHW sp;
    um_nvs_get_ot_dhw_enabled(&enableHotWater);
    um_nvs_get_ot_dhw_setpoint(&targetDHWTemp);
    um_nvs_get_ot_ch_setpoint(&targetCHTemp);
    uint8_t mod = 0;
    um_nvs_get_ot_modulation(&mod);
    uint8_t hcr = 0;
    um_nvs_get_ot_heating_curve_ratio(&hcr);

    ot_data.otdhwsp = targetDHWTemp;
    ot_data.ottbsp = targetCHTemp;
    ot_data.otch = enableCentralHeating;
    
    ot_data.mod = mod;
    ot_data.othcr = hcr;

    ot_data.hwa = enableHotWater;

    xTaskCreatePinnedToCore(um_opentherm_control_task_handler, TAG, configMINIMAL_STACK_SIZE * 4, NULL, 2, &ot_handle, 1);
}

um_ot_data_t um_ot_get_data()
{
    return ot_data;
}

void um_ot_reset_error()
{
    needReset = true;
}

void um_ot_set_central_heating_active(bool state)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_CH, state ? 1 : 0);
    enableCentralHeating = state;
    ot_data.otch = state;
}

void um_ot_set_heat_curve_ratio(int ratio)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_HCR, ratio);
    ot_data.othcr = ratio;
}

void um_ot_update_state(bool otch, int otdhwsp, int ottbsp)
{
    um_ot_set_central_heating_active(otch);

    targetCHTemp = ottbsp;
    ot_data.ottbsp = ottbsp;

    targetDHWTemp = otdhwsp;
    ot_data.otdhwsp = otdhwsp;
}

#endif