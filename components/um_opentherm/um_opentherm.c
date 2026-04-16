#include "esp_log.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"

#include "base_config.h"
#include "um_nvs.h"
#include "um_events.h"
#include "um_opentherm.h"
#include "cJSON.h"

// CONFIG_UM_CFG_OT_IN_GPIO=26
// CONFIG_UM_CFG_OT_OUT_GPIO=25

#if UM_FEATURE_ENABLED(OPENTHERM)

#define ESP_INTR_FLAG_DEFAULT 0

// Тайминги (в миллисекундах)
#define OT_KEEPALIVE_INTERVAL_MS 200       // Keep-alive каждые 500 мс
#define OT_SENSOR_READ_INTERVAL_MS 60000   // Сенсоры каждые 60 секунд
#define OT_CONFIG_READ_INTERVAL_MS 3600000 // Конфигурация каждый час
#define OT_MAIN_LOOP_DELAY_MS 300          // Основная задержка цикла
#define OT_ERROR_RETRY_DELAY_MS 5000       // Задержка при ошибке
#define OT_COMMAND_TIMEOUT_MS 100          // Таймаут команды

static bool communication_established = false;
static uint8_t comm_fail_count = 0;
#define COMM_FAIL_LIMIT 3

static uint8_t targetDHWTemp = 59;
static uint8_t targetCHTemp = 60;
static bool otEnabled = false;
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
static unsigned char task_count_max_to_send_data = 120; // ~30 секунд при цикле 250 мс

// Флаги для отслеживания изменений (keep-alive не требует отдельного флага)
static bool last_ch_en = false;
static bool last_dhw_en = false;
static bool last_otc_en = false;
static bool last_cooling_en = false;
static bool last_ch2_en = false;
static float last_ch_sp = 0;
static float last_dhw_sp = 0;

// Таймеры для периодических операций
static TickType_t last_keepalive = 0;
static TickType_t last_sensor_read = 0;
static TickType_t last_config_read = 0;

// Счетчик ошибок подряд
static uint8_t consecutive_errors = 0;
#define MAX_CONSECUTIVE_ERRORS 5

// Флаг первого запуска для чтения конфигурации
static bool first_run = true;

void um_opentherm_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (id != UMNI_EVENT_OPENTHERM_CH_ON && id != UMNI_EVENT_OPENTHERM_CH_OFF)
    {
        return;
    }
    bool ch_en = (id == UMNI_EVENT_OPENTHERM_CH_ON);

    um_ot_update_state(ch_en, ot_data.dhw_sp, ot_data.ch_sp);
    ESP_LOGI(TAG, "OT CH triggered by event. OT is %s", ch_en ? "ON" : "OFF");
}

/**
 * @brief Чтение конфигурации котла (выполняется один раз при старте)
 */
static void um_ot_read_slave_configuration(void)
{
    ot_data.slave_config = esp_ot_get_slave_configuration();
    taskYIELD();
    vTaskDelay(pdMS_TO_TICKS(10));
    ot_data.slave_ot_version = esp_ot_get_slave_ot_version();
    taskYIELD();
    vTaskDelay(pdMS_TO_TICKS(10));
    ot_data.slave_product_version = esp_ot_get_slave_product_version();
    taskYIELD();
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "====== MAIN FUNCTIONS ======");
    ESP_LOGI(TAG, "CONTROL TYPE %s", ot_data.slave_config.control_type == 0 ? "ON/OFF" : "MODULATING");
    ESP_LOGI(TAG, "DHW %s", ot_data.slave_config.dhw_present ? "SUPPORTED" : "NOT SUPPORTED");
    ESP_LOGI(TAG, "DHW CONFIG %s", ot_data.slave_config.dhw_config ? "STORAGE TANK" : "INSTANTANEOUS");
    ESP_LOGI(TAG, "CH2 %s", ot_data.slave_config.ch2_present ? "SUPPORTED" : "NOT SUPPORTED");
    ESP_LOGI(TAG, "COOLING %s", ot_data.slave_config.cooling_supported ? "SUPPORTED" : "NOT SUPPORTED");
    ESP_LOGI(TAG, "PUMP CONTROL %s", ot_data.slave_config.pump_control_allowed ? "ALLOWED" : "NOT ALLOWED");

    ESP_LOGI(TAG, "====== SLAVE INFO ======");
    ESP_LOGI(TAG, "Slave OT Version: %.1f", ot_data.slave_ot_version);
    ESP_LOGI(TAG, "Slave Version: %08lX", ot_data.slave_product_version);
}

/**
 * @brief Периодическое чтение сенсоров (температуры, давление, модуляция)
 */
static void um_ot_read_sensors(void)
{
    // Если связь с котлом не установлена - пропускаем чтение
    if (!communication_established)
    {
        ESP_LOGD(TAG, "Skipping sensor read - no communication");
        return;
    }

    // Чтение основных сенсоров с задержками между вызовами
    ot_data.boiler_temperature = esp_ot_get_boiler_temperature();
    vTaskDelay(pdMS_TO_TICKS(20)); // Увеличена задержка
    taskYIELD();

    ot_data.return_temperature = esp_ot_get_return_temperature();
    vTaskDelay(pdMS_TO_TICKS(20));
    taskYIELD();

    if (ot_data.slave_config.dhw_present)
    {
        ot_data.dhw_temperature = esp_ot_get_dhw_temperature();
        vTaskDelay(pdMS_TO_TICKS(20));
        taskYIELD();

        ot_data.dhw_setpoint = esp_ot_get_dhw_setpoint();
        vTaskDelay(pdMS_TO_TICKS(20));
        taskYIELD();

        ot_data.flow_rate = esp_ot_get_flow_rate();
        vTaskDelay(pdMS_TO_TICKS(20));
        taskYIELD();
    }

    ot_data.modulation = esp_ot_get_modulation();
    vTaskDelay(pdMS_TO_TICKS(20));
    taskYIELD();

    ot_data.pressure = esp_ot_get_pressure();
    vTaskDelay(pdMS_TO_TICKS(20));
    taskYIELD();

    ot_data.outside_temperature = esp_ot_get_outside_temperature();
    vTaskDelay(pdMS_TO_TICKS(20));
    taskYIELD();

    if (ot_data.slave_config.ch2_present)
    {
        ot_data.flow_rate_ch2 = esp_ot_get_ch2_flow();
        vTaskDelay(pdMS_TO_TICKS(20));
        taskYIELD();
    }

    ESP_LOGI(TAG, "=== OPENTHERM STATUS ===");
    ESP_LOGI(TAG, "Boiler: CH=%s, DHW=%s, Flame=%s, Fault=%s, Diag=%s",
             ot_data.central_heating_active ? "ON" : "OFF",
             ot_data.hot_water_active ? "ON" : "OFF",
             ot_data.flame_on ? "ON" : "OFF",
             ot_data.is_fault ? "YES" : "NO",
             esp_ot_is_diagnostic(status) ? "YES" : "NO");
    ESP_LOGI(TAG, "Temps: CH=%.1f°C (set=%.0f), Return=%.1f°C (Δ=%.1f), DHW=%.1f°C (set=%.0f), Outside=%.1f°C",
             ot_data.boiler_temperature, targetCHTemp,
             ot_data.return_temperature, ot_data.boiler_temperature - ot_data.return_temperature,
             ot_data.dhw_temperature, targetDHWTemp,
             ot_data.outside_temperature);
    ESP_LOGI(TAG, "Params: Mod=%.1f%%, Press=%.1f bar, Flow=%.1f l/min",
             ot_data.modulation, ot_data.pressure, ot_data.flow_rate);
    ESP_LOGI(TAG, "Config: Type=%s, DHW=%s, Range CH=%d..%d°C, Cap=%dkW, MinMod=%d%%",
             ot_data.slave_config.control_type == 0 ? "MOD" : "ON/OFF",
             ot_data.slave_config.dhw_present ? "yes" : "no",
             ot_data.ch_min_max.min, ot_data.ch_min_max.max,
             ot_data.cap_mod.kw, ot_data.cap_mod.min_modulation);
    if (ot_data.is_fault)
    {
        ESP_LOGE(TAG, "FAULT: code=%d, diag=%d, service=%d, reset=%d",
                 ot_data.asf_flags.fault_code, ot_data.asf_flags.diag_code,
                 ot_data.asf_flags.is_service_request, ot_data.asf_flags.can_reset);
    }
    ESP_LOGI(TAG, "========================");
}

/**
 * @brief Периодическое чтение конфигурационных параметров (bounds, capacity)
 */
static void um_ot_read_configuration(void)
{
    // Чтение bounds для CH и DHW
    ot_data.ch_min_max = esp_ot_get_ch_upper_lower_bounds();
    vTaskDelay(pdMS_TO_TICKS(10));

    if (ot_data.slave_config.dhw_present)
    {
        ot_data.dhw_min_max = esp_ot_get_dhw_upper_lower_bounds();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Чтение максимальной мощности и минимальной модуляции
    ot_data.cap_mod = esp_ot_get_max_capacity_min_modulation();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Чтение bounds кривой нагрева (для OTC)
    ot_data.curve_bounds = esp_ot_get_heat_curve_ul_bounds();
    vTaskDelay(pdMS_TO_TICKS(10));

    // Чтение максимальной уставки CH
    ot_data.ch_max_setpoint = esp_ot_get_ch_max_setpoint();

    ESP_LOGI(TAG, "Configuration: CH bounds [%d..%d]°C, DHW bounds [%d..%d]°C",
             ot_data.ch_min_max.min, ot_data.ch_min_max.max,
             ot_data.dhw_min_max.min, ot_data.dhw_min_max.max);
    ESP_LOGI(TAG, "Capacity: %d kW, Min modulation: %d%%",
             ot_data.cap_mod.kw, ot_data.cap_mod.min_modulation);
}

/**
 * @brief Отправка статуса master (WRITE-DATA id=0)
 */
static bool um_ot_send_master_status(void)
{
    vTaskDelay(pdMS_TO_TICKS(20));

    unsigned long response = esp_ot_set_boiler_status(
        enableCentralHeating,
        enableHotWater,
        enableCooling,
        enableOutsideTemperatureCompensation,
        enableCentralHeating2);

    open_therm_response_status_t resp_status = esp_ot_get_last_response_status();

    if (resp_status == OT_STATUS_SUCCESS && esp_ot_is_valid_response(response))
    {
        // Связь установлена
        if (!communication_established)
        {
            communication_established = true;
            ot_data.adapter_success = true;
            ESP_LOGI(TAG, "Communication with boiler established");

            // for (int i = 0; i < 3; i++)
            //{
            um_ot_read_slave_configuration();
            vTaskDelay(pdMS_TO_TICKS(50));
            //}
        }
        comm_fail_count = 0;

        ot_data.central_heating_active = esp_ot_is_central_heating_active(response);
        ot_data.hot_water_active = esp_ot_is_hot_water_active(response);
        ot_data.flame_on = esp_ot_is_flame_on(response);
        ot_data.is_fault = esp_ot_is_fault(response);
        return true;
    }

    // Логируем только первые ошибки
    if (comm_fail_count < COMM_FAIL_LIMIT)
    {
        ESP_LOGW(TAG, "Failed to send master status, response: %d", resp_status);
    }
    else if (comm_fail_count == COMM_FAIL_LIMIT)
    {
        ESP_LOGW(TAG, "No boiler detected, entering low-communication mode");
        communication_established = false;
        ot_data.adapter_success = false;
    }

    comm_fail_count++;
    return false;
}

/**
 * @brief Отправка уставки CH температуры
 */
static bool um_ot_send_ch_setpoint(float setpoint)
{
    // Проверка bounds
    if (setpoint < ot_data.ch_min_max.min)
        setpoint = ot_data.ch_min_max.min;
    if (setpoint > ot_data.ch_min_max.max)
        setpoint = ot_data.ch_min_max.max;

    bool result = esp_ot_set_boiler_temperature(setpoint);
    if (result)
    {
        ESP_LOGI(TAG, "CH setpoint sent: %.0f°C", setpoint);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to send CH setpoint: %.0f°C", setpoint);
    }

    return result;
}

/**
 * @brief Отправка уставки DHW температуры
 */
static bool um_ot_send_dhw_setpoint(float setpoint)
{
    if (!ot_data.slave_config.dhw_present)
    {
        ESP_LOGD(TAG, "DHW not supported, skipping setpoint");
        return false;
    }

    // Проверка bounds
    if (setpoint < ot_data.dhw_min_max.min)
        setpoint = ot_data.dhw_min_max.min;
    if (setpoint > ot_data.dhw_min_max.max)
        setpoint = ot_data.dhw_min_max.max;

    bool result = esp_ot_set_dhw_setpoint(setpoint);
    if (result)
    {
        ESP_LOGI(TAG, "DHW setpoint sent: %.0f°C", setpoint);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to send DHW setpoint: %.0f°C", setpoint);
    }

    return result;
}

/**
 * @brief Обработка ошибок и fault-флагов
 */
static void um_ot_handle_faults(void)
{
    if (!ot_data.is_fault)
        return;

    // Получаем реальные флаги ошибок
    esp_ot_asf_flags_t flags = esp_ot_get_asf_flags();

    // Проверяем: если все флаги = 0 и fault_code = 0 и diag_code = 0 - это ложное срабатывание
    if (flags.fault_code == 0 && flags.diag_code == 0 &&
        !flags.is_service_request && !flags.can_reset &&
        !flags.is_low_water_press && !flags.is_gas_flame_fault &&
        !flags.is_air_press_fault && !flags.is_water_over_temp)
    {
        ESP_LOGW(TAG, "False fault detected (all flags zero), ignoring");
        ot_data.is_fault = false; // Сбрасываем ложный fault
        return;
    }

    // Настоящая ошибка - логируем
    ot_data.asf_flags = flags;
    ot_data.fault_code = flags.fault_code;

    ESP_LOGE(TAG, "=== FAULT DETECTED ===");
    ESP_LOGE(TAG, "Fault code: %d, Diagnostic code: %d", flags.fault_code, flags.diag_code);
    ESP_LOGE(TAG, "Service request: %d", flags.is_service_request);
    ESP_LOGE(TAG, "Can reset: %d", flags.can_reset);
    ESP_LOGE(TAG, "Low water pressure: %d", flags.is_low_water_press);
    ESP_LOGE(TAG, "Gas/flame fault: %d", flags.is_gas_flame_fault);
    ESP_LOGE(TAG, "Air pressure fault: %d", flags.is_air_press_fault);
    ESP_LOGE(TAG, "Water over temperature: %d", flags.is_water_over_temp);

    if (needReset && flags.can_reset)
    {
        ot_reset();
        needReset = false;
        ESP_LOGI(TAG, "Reset command sent");
    }
}

/**
 * @brief Проверка и применение изменений параметров
 */
static void um_ot_apply_pending_changes(void)
{
    bool need_ch_setpoint = false;
    bool need_dhw_setpoint = false;

    // Проверка изменений статуса master
    if (enableCentralHeating != last_ch_en)
    {
        last_ch_en = enableCentralHeating;
        ESP_LOGI(TAG, "CH enable changed to: %d", enableCentralHeating);
    }

    if (enableHotWater != last_dhw_en)
    {
        last_dhw_en = enableHotWater;
        ESP_LOGI(TAG, "DHW enable changed to: %d", enableHotWater);
    }

    if (enableCooling != last_cooling_en)
    {
        last_cooling_en = enableCooling;
        ESP_LOGI(TAG, "Cooling enable changed to: %d", enableCooling);
    }

    if (enableOutsideTemperatureCompensation != last_otc_en)
    {
        last_otc_en = enableOutsideTemperatureCompensation;
        ESP_LOGI(TAG, "OTC enable changed to: %d", enableOutsideTemperatureCompensation);
    }

    if (enableCentralHeating2 != last_ch2_en)
    {
        last_ch2_en = enableCentralHeating2;
        ESP_LOGI(TAG, "CH2 enable changed to: %d", enableCentralHeating2);
    }

    // Проверка изменений уставок
    if ((float)targetCHTemp != last_ch_sp)
    {
        last_ch_sp = targetCHTemp;
        need_ch_setpoint = true;
        ESP_LOGI(TAG, "CH setpoint changed to: %d°C", targetCHTemp);
    }

    if ((float)targetDHWTemp != last_dhw_sp)
    {
        last_dhw_sp = targetDHWTemp;
        need_dhw_setpoint = true;
        ESP_LOGI(TAG, "DHW setpoint changed to: %d°C", targetDHWTemp);
    }

    if (need_ch_setpoint)
    {
        if (!um_ot_send_ch_setpoint(targetCHTemp))
        {
            consecutive_errors++;
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(OT_COMMAND_TIMEOUT_MS));
    }

    if (need_dhw_setpoint && ot_data.slave_config.dhw_present)
    {
        if (!um_ot_send_dhw_setpoint(targetDHWTemp))
        {
            consecutive_errors++;
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(OT_COMMAND_TIMEOUT_MS));
    }

    // Сброс счетчика ошибок при успехе
    if (need_ch_setpoint || need_dhw_setpoint)
    {
        consecutive_errors = 0;
    }
}

/**
 * @brief Основная задача управления OpenTherm
 */
/**
 * @brief Основная задача управления OpenTherm
 */
void um_opentherm_control_task_handler(void *pvParameter)
{
    // Инициализация начальных значений из NVS
    um_nvs_get_ot_enabled(&otEnabled);
    um_nvs_get_ot_dhw_setpoint(&targetDHWTemp);
    um_nvs_get_ot_ch_setpoint(&targetCHTemp);
    um_nvs_get_ot_ch_enabled(&enableCentralHeating);
    um_nvs_get_ot_dhw_enabled(&enableHotWater);
    um_nvs_get_ot_outdoor_temp_comp(&enableOutsideTemperatureCompensation);

    // Инициализация last-значений
    last_ch_en = enableCentralHeating;
    last_dhw_en = enableHotWater;
    last_otc_en = enableOutsideTemperatureCompensation;
    last_cooling_en = enableCooling;
    last_ch2_en = enableCentralHeating2;
    last_ch_sp = targetCHTemp;
    last_dhw_sp = targetDHWTemp;

    // Флаги состояния
    bool config_read_done = false;
    bool last_enabled_state = false;
    TickType_t last_nvs_check_time = xTaskGetTickCount();
    TickType_t last_sensor_read = 0;

    ESP_LOGI(TAG, "OpenTherm control task started");

    while (true)
    {
        TickType_t loop_start_time = xTaskGetTickCount();

        // ============================================
        // 1. Проверка состояния OT (включен/выключен)
        // ============================================
        if (!otEnabled)
        {
            if (last_enabled_state != otEnabled)
            {
                ESP_LOGI(TAG, "OpenTherm disabled, entering low-power mode");
                last_enabled_state = otEnabled;
                config_read_done = false;
                first_run = true;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));

            TickType_t current_time = xTaskGetTickCount();
            if ((current_time - last_nvs_check_time) > pdMS_TO_TICKS(5000))
            {
                um_nvs_get_ot_enabled(&otEnabled);
                last_nvs_check_time = current_time;

                if (otEnabled)
                {
                    ESP_LOGI(TAG, "OpenTherm enabled from NVS, initializing...");
                    um_nvs_get_ot_dhw_setpoint(&targetDHWTemp);
                    um_nvs_get_ot_ch_setpoint(&targetCHTemp);
                    um_nvs_get_ot_ch_enabled(&enableCentralHeating);
                    um_nvs_get_ot_dhw_enabled(&enableHotWater);
                    um_nvs_get_ot_outdoor_temp_comp(&enableOutsideTemperatureCompensation);

                    last_ch_sp = targetCHTemp;
                    last_dhw_sp = targetDHWTemp;
                    last_ch_en = enableCentralHeating;
                    last_dhw_en = enableHotWater;
                }
            }
            continue;
        }

        // ============================================
        // 2. Первый запуск - чтение конфигурации (3 попытки)
        // ============================================
        if (first_run || !config_read_done)
        {
            if (!um_ot_send_master_status())
            {
                ESP_LOGW(TAG, "Boiler not responding during init. Retry in 5s...");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue; // Выходим из цикла и пробуем снова через 5 сек
            }

            consecutive_errors = 0;

            ESP_LOGI(TAG, "Reading boiler configuration (3 attempts)...");

            for (int i = 0; i < 3; i++)
            {
                um_ot_read_slave_configuration();
                vTaskDelay(pdMS_TO_TICKS(2000));
            }

            um_ot_read_configuration();
            vTaskDelay(pdMS_TO_TICKS(50));
            um_ot_detect_supported_features();
            vTaskDelay(pdMS_TO_TICKS(50));

            config_read_done = true;
            first_run = false;

            um_ot_send_ch_setpoint(targetCHTemp);
            vTaskDelay(pdMS_TO_TICKS(OT_COMMAND_TIMEOUT_MS));

            if (ot_data.slave_config.dhw_present)
            {
                um_ot_send_dhw_setpoint(targetDHWTemp);
            }

            ot_data.ready = true;
            continue;
        }

        // ============================================
        // 3. ОСНОВНОЙ ЦИКЛ: отправляем статус master КАЖДЫЙ ЦИКЛ
        // ============================================
        if (!um_ot_send_master_status())
        {
            // Если связи нет (вернулся false/timeout)
            ESP_LOGW(TAG, "Boiler not responding. Sleeping for 5 seconds...");

            // Вместо того чтобы сразу долбиться снова,
            // засыпаем на 5 секунд. Это полностью разгрузит CPU.
            vTaskDelay(pdMS_TO_TICKS(5000));

            // Можно пропустить остальную часть цикла
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        // ============================================
        // 4. Применение изменений параметров (уставки)
        // ============================================
        um_ot_apply_pending_changes();
        vTaskDelay(pdMS_TO_TICKS(10));

        // ============================================
        // 5. Чтение сенсоров (раз в 10 секунд)
        // ============================================
        TickType_t now = xTaskGetTickCount();
        if (communication_established)
        {
            if ((now - last_sensor_read) >= pdMS_TO_TICKS(OT_SENSOR_READ_INTERVAL_MS))
            {
                um_ot_read_sensors();
                last_sensor_read = now;
            }
        }

        // ============================================
        // 6. Обработка fault-флагов
        // ============================================
        um_ot_handle_faults();

        // ============================================
        // 7. Публикация MQTT
        // ============================================
        if (task_count >= task_count_max_to_send_data)
        {
            task_count = 0;
            um_event_publish(UMNI_EVENT_OPENTHERM_SET_DATA, NULL, 0, portMAX_DELAY);
            ESP_LOGD(TAG, "MQTT data published");
        }
        else
        {
            task_count++;
        }

        // ============================================
        // 8. Периодическая проверка NVS
        // ============================================
        now = xTaskGetTickCount();
        if ((now - last_nvs_check_time) > pdMS_TO_TICKS(5000))
        {
            bool new_ot_enabled;
            um_nvs_get_ot_enabled(&new_ot_enabled);

            if (new_ot_enabled != otEnabled)
            {
                otEnabled = new_ot_enabled;
                ESP_LOGI(TAG, "OT enabled state changed to: %d", otEnabled);

                if (!otEnabled)
                {
                    config_read_done = false;
                    first_run = true;
                    ESP_LOGI(TAG, "OpenTherm disabled from NVS, stopping operations");
                }
            }

            uint8_t new_dhw_sp, new_ch_sp;
            bool new_ch_en, new_dhw_en, new_otc_en;

            um_nvs_get_ot_dhw_setpoint(&new_dhw_sp);
            um_nvs_get_ot_ch_setpoint(&new_ch_sp);
            um_nvs_get_ot_ch_enabled(&new_ch_en);
            um_nvs_get_ot_dhw_enabled(&new_dhw_en);
            um_nvs_get_ot_outdoor_temp_comp(&new_otc_en);

            if (new_dhw_sp != targetDHWTemp)
                targetDHWTemp = new_dhw_sp;
            if (new_ch_sp != targetCHTemp)
                targetCHTemp = new_ch_sp;
            if (new_ch_en != enableCentralHeating)
                enableCentralHeating = new_ch_en;
            if (new_dhw_en != enableHotWater)
                enableHotWater = new_dhw_en;
            if (new_otc_en != enableOutsideTemperatureCompensation)
                enableOutsideTemperatureCompensation = new_otc_en;

            last_nvs_check_time = now;
        }

        // ============================================
        // 9. Задержка цикла
        // ============================================
        TickType_t loop_elapsed = xTaskGetTickCount() - loop_start_time;
        TickType_t loop_delay_ticks = pdMS_TO_TICKS(OT_MAIN_LOOP_DELAY_MS);

        if (loop_elapsed < loop_delay_ticks)
        {
            vTaskDelay(loop_delay_ticks - loop_elapsed);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    vTaskDelete(NULL);
}

// ============================================
// Публичные функции (API)
// ============================================

esp_err_t um_ot_set_boiler_status(
    bool enable_central_heating,
    bool enable_hot_water,
    bool enable_cooling,
    bool enable_outside_temperature_compensation,
    bool enable_central_heating2)
{
    enableCentralHeating = enable_central_heating;
    enableHotWater = enable_hot_water;
    enableCooling = enable_cooling;
    enableOutsideTemperatureCompensation = enable_outside_temperature_compensation;
    enableCentralHeating2 = enable_central_heating2;

    ot_data.ch_en = enableCentralHeating;

    // Статус будет отправлен в следующем цикле apply_pending_changes
    return ESP_OK;
}

void um_ot_set_boiler_temp(float temp)
{
    if (temp < 0)
        temp = 0;
    if (temp > 100)
        temp = 100;
    targetCHTemp = (uint8_t)temp;
    um_nvs_write_i8(UM_NVS_KEY_OT_CH_SETPOINT, targetCHTemp);
    ESP_LOGI(TAG, "CH temperature requested: %.0f°C", temp);
}

void um_ot_set_dhw_setpoint(float temp)
{
    if (temp < 0)
        temp = 0;
    if (temp > 100)
        temp = 100;
    targetDHWTemp = (uint8_t)temp;
    um_nvs_write_i8(UM_NVS_KEY_OT_DHW_SETPOINT, targetDHWTemp);
    ESP_LOGI(TAG, "DHW setpoint requested: %.0f°C", temp);
}

void um_ot_init()
{
    um_event_subscribe(UMNI_EVENT_ANY, um_opentherm_event_handler, NULL);

    esp_ot_init(
        CONFIG_UM_CFG_OT_IN_GPIO,
        CONFIG_UM_CFG_OT_OUT_GPIO,
        false, // Master mode
        NULL);

    vTaskDelay(pdMS_TO_TICKS(500));

    // Инициализация структуры ot_data
    memset(&ot_data, 0, sizeof(um_ot_data_t));

    // Чтение начальных значений из NVS
    um_nvs_get_ot_dhw_enabled(&enableHotWater);
    um_nvs_get_ot_dhw_setpoint(&targetDHWTemp);
    um_nvs_get_ot_ch_enabled(&enableCentralHeating);
    um_nvs_get_ot_ch_setpoint(&targetCHTemp);

    uint8_t mod = 0;
    um_nvs_get_ot_modulation(&mod);
    uint8_t hcr = 0;
    um_nvs_get_ot_heating_curve_ratio(&hcr);

    ot_data.dhw_en = enableHotWater;
    ot_data.dhw_sp = targetDHWTemp;
    ot_data.ch_sp = targetCHTemp;
    ot_data.ch_en = enableCentralHeating;
    ot_data.mod = mod;
    ot_data.hcr = hcr;

    // Создание задачи управления
    xTaskCreatePinnedToCore(
        um_opentherm_control_task_handler,
        TAG,
        configMINIMAL_STACK_SIZE * 6, // Увеличен стек
        NULL,
        1, // Приоритет
        &ot_handle,
        1); // Ядро 1

    ESP_LOGI(TAG, "OpenTherm initialized");
}

um_ot_data_t um_ot_get_data()
{
    return ot_data;
}

void um_ot_reset_error()
{
    needReset = true;
    ESP_LOGI(TAG, "Reset error flag set");
}

void um_ot_set_central_heating_active(bool state)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_CH, state ? 1 : 0);
    enableCentralHeating = state;
    ot_data.ch_en = state;
    ESP_LOGI(TAG, "Central heating active: %d", state);
}

void um_ot_set_hot_water_active(bool state)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_DHW, state ? 1 : 0);
    enableHotWater = state;
    ot_data.dhw_en = state;
    ESP_LOGI(TAG, "Hot water active: %d", state);
}

void um_ot_set_ch2(bool active)
{
    if (!ot_data.slave_config.ch2_present)
    {
        ESP_LOGW(TAG, "CH2 not supported by boiler");
        return;
    }
    enableCentralHeating2 = active;
    um_nvs_write_i8(UM_NVS_KEY_OT_CH2, active ? 1 : 0);
    ESP_LOGI(TAG, "CH2 active: %d", active);
}

void um_ot_set_outside_temp_comp(bool state)
{
    um_nvs_write_i8(UM_NVS_KEY_OT_OTC, state ? 1 : 0);
    enableOutsideTemperatureCompensation = state;
    ot_data.otc_en = state;
    ESP_LOGI(TAG, "Outside temperature compensation: %d", state);
}

void um_ot_set_modulation_level(int level)
{
    if (level < 0)
        level = 0;
    if (level > 99)
        level = 99;

    // Проверка поддержки!
    if (!ot_data.supported.modulation_write)
    {
        ESP_LOGW(TAG, "Modulation level write NOT supported by boiler, ignoring");
        return;
    }

    um_nvs_write_i8(UM_NVS_KEY_OT_MOD, level);
    ot_data.mod = level;
    ESP_LOGI(TAG, "Modulation level set: %d%%", level);

    if (otEnabled && ot_data.ready)
    {
        esp_ot_set_modulation_level(level);
    }
}

void um_ot_set_heat_curve_ratio(int ratio)
{
    if (ratio < 0)
        ratio = 0;
    if (ratio > 100)
        ratio = 100;

    // Проверка поддержки!
    if (!ot_data.supported.heat_curve_write)
    {
        ESP_LOGW(TAG, "Heat curve ratio write NOT supported by boiler, ignoring");
        return;
    }

    um_nvs_write_i8(UM_NVS_KEY_OT_HCR, ratio);
    ot_data.hcr = ratio;
    ot_data.heat_curve_ratio = ratio;
    ESP_LOGI(TAG, "Heat curve ratio set: %d", ratio);
}

void um_ot_update_state(bool otch, int otdhwsp, int ottbsp)
{
    um_ot_set_central_heating_active(otch);

    targetCHTemp = ottbsp;
    ot_data.ch_sp = ottbsp;

    targetDHWTemp = otdhwsp;
    ot_data.dhw_sp = otdhwsp;

    ESP_LOGI(TAG, "State updated: CH=%d, CH_SP=%d°C, DHW_SP=%d°C",
             otch, ottbsp, otdhwsp);
}

/**
 * @brief Получить JSON-строку со всеми данными состояния OpenTherm
 * @return char* JSON-строка (должна быть освобождена через free() после использования)
 *         или NULL при ошибке
 *
 * ВНИМАНИЕ! Вызывающий обязан освободить память: free(json_string);
 */
char *um_ot_get_status_json(void)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return NULL;
    }

    // Основные флаги состояния
    cJSON_AddBoolToObject(root, "ot_enabled", otEnabled);
    cJSON_AddBoolToObject(root, "ready", ot_data.ready);
    cJSON_AddBoolToObject(root, "adapter_success", ot_data.adapter_success);
    cJSON_AddNumberToObject(root, "status_code", ot_data.status);

    // Управляющие параметры (что мы задаем)
    cJSON_AddBoolToObject(root, "ch_enable", enableCentralHeating);
    cJSON_AddNumberToObject(root, "ch_setpoint_requested", targetCHTemp);
    cJSON_AddBoolToObject(root, "dhw_enable", enableHotWater);
    cJSON_AddNumberToObject(root, "dhw_setpoint_requested", targetDHWTemp);
    cJSON_AddBoolToObject(root, "otc_enable", enableOutsideTemperatureCompensation);
    cJSON_AddBoolToObject(root, "cooling_enable", enableCooling);
    cJSON_AddBoolToObject(root, "ch2_enable", enableCentralHeating2);
    cJSON_AddNumberToObject(root, "modulation_level_set", ot_data.mod);
    cJSON_AddNumberToObject(root, "heat_curve_ratio", ot_data.hcr);

    // Обратная связь от котла
    cJSON_AddBoolToObject(root, "central_heating_active", ot_data.central_heating_active);
    cJSON_AddBoolToObject(root, "hot_water_active", ot_data.hot_water_active);
    cJSON_AddBoolToObject(root, "flame_on", ot_data.flame_on);
    cJSON_AddBoolToObject(root, "is_fault", ot_data.is_fault);

    // Температуры
    cJSON_AddNumberToObject(root, "boiler_temperature", ot_data.boiler_temperature);
    cJSON_AddNumberToObject(root, "return_temperature", ot_data.return_temperature);
    cJSON_AddNumberToObject(root, "dhw_temperature", ot_data.dhw_temperature);
    cJSON_AddNumberToObject(root, "outside_temperature", ot_data.outside_temperature);
    cJSON_AddNumberToObject(root, "dhw_setpoint_current", ot_data.dhw_setpoint);
    cJSON_AddNumberToObject(root, "ch_max_setpoint", ot_data.ch_max_setpoint);

    // Другие параметры
    cJSON_AddNumberToObject(root, "modulation", ot_data.modulation);
    cJSON_AddNumberToObject(root, "pressure", ot_data.pressure);
    cJSON_AddNumberToObject(root, "flow_rate", ot_data.flow_rate);
    cJSON_AddNumberToObject(root, "flow_rate_ch2", ot_data.flow_rate_ch2);
    cJSON_AddNumberToObject(root, "fault_code", ot_data.fault_code);

    // Конфигурация котла (что поддерживает)
    cJSON *config = cJSON_CreateObject();
    if (config)
    {
        cJSON_AddStringToObject(config, "control_type",
                                ot_data.slave_config.control_type == 0 ? "ON/OFF" : "MODULATING");
        cJSON_AddBoolToObject(config, "dhw_present", ot_data.slave_config.dhw_present);
        cJSON_AddStringToObject(config, "dhw_config",
                                ot_data.slave_config.dhw_config ? "STORAGE_TANK" : "INSTANTANEOUS");
        cJSON_AddBoolToObject(config, "ch2_present", ot_data.slave_config.ch2_present);
        cJSON_AddBoolToObject(config, "cooling_supported", ot_data.slave_config.cooling_supported);
        cJSON_AddBoolToObject(config, "pump_control_allowed", ot_data.slave_config.pump_control_allowed);
        cJSON_AddNumberToObject(config, "slave_ot_version", ot_data.slave_ot_version);
        cJSON_AddNumberToObject(config, "slave_product_version", ot_data.slave_product_version);
        cJSON_AddItemToObject(root, "boiler_config", config);
    }

    // Диапазоны (bounds)
    cJSON *bounds = cJSON_CreateObject();
    if (bounds)
    {
        cJSON *ch_bounds = cJSON_CreateObject();
        cJSON_AddNumberToObject(ch_bounds, "min", ot_data.ch_min_max.min);
        cJSON_AddNumberToObject(ch_bounds, "max", ot_data.ch_min_max.max);
        cJSON_AddItemToObject(bounds, "ch", ch_bounds);

        cJSON *dhw_bounds = cJSON_CreateObject();
        cJSON_AddNumberToObject(dhw_bounds, "min", ot_data.dhw_min_max.min);
        cJSON_AddNumberToObject(dhw_bounds, "max", ot_data.dhw_min_max.max);
        cJSON_AddItemToObject(bounds, "dhw", dhw_bounds);

        cJSON *curve_bounds = cJSON_CreateObject();
        cJSON_AddNumberToObject(curve_bounds, "min", ot_data.curve_bounds.min);
        cJSON_AddNumberToObject(curve_bounds, "max", ot_data.curve_bounds.max);
        cJSON_AddItemToObject(bounds, "heat_curve", curve_bounds);

        cJSON_AddItemToObject(root, "bounds", bounds);
    }

    // ASF флаги ошибок (если есть)
    if (ot_data.is_fault)
    {
        cJSON *faults = cJSON_CreateObject();
        if (faults)
        {
            cJSON_AddBoolToObject(faults, "service_request", ot_data.asf_flags.is_service_request);
            cJSON_AddBoolToObject(faults, "can_reset", ot_data.asf_flags.can_reset);
            cJSON_AddBoolToObject(faults, "low_water_pressure", ot_data.asf_flags.is_low_water_press);
            cJSON_AddBoolToObject(faults, "gas_flame_fault", ot_data.asf_flags.is_gas_flame_fault);
            cJSON_AddBoolToObject(faults, "air_pressure_fault", ot_data.asf_flags.is_air_press_fault);
            cJSON_AddBoolToObject(faults, "water_over_temp", ot_data.asf_flags.is_water_over_temp);
            cJSON_AddNumberToObject(faults, "oem_fault_code", ot_data.asf_flags.fault_code);
            cJSON_AddNumberToObject(faults, "oem_diagnostic_code", ot_data.asf_flags.diag_code);
            cJSON_AddItemToObject(root, "faults", faults);
        }
    }

    cJSON *supported = cJSON_CreateObject();
    if (supported)
    {
        cJSON_AddBoolToObject(supported, "modulation_read", ot_data.supported.modulation);
        cJSON_AddBoolToObject(supported, "modulation_write", ot_data.supported.modulation_write);
        cJSON_AddBoolToObject(supported, "heat_curve_read", ot_data.supported.heat_curve);
        cJSON_AddBoolToObject(supported, "heat_curve_write", ot_data.supported.heat_curve_write);
        cJSON_AddBoolToObject(supported, "outside_temperature", ot_data.supported.outside_temp);
        cJSON_AddBoolToObject(supported, "return_temperature", ot_data.supported.return_temp);
        cJSON_AddBoolToObject(supported, "pressure", ot_data.supported.pressure);
        cJSON_AddBoolToObject(supported, "flow_rate", ot_data.supported.flow_rate);
        cJSON_AddBoolToObject(supported, "dhw_present", ot_data.slave_config.dhw_present);
        cJSON_AddBoolToObject(supported, "modulating", ot_data.slave_config.control_type == 0);
        cJSON_AddItemToObject(root, "supported_features", supported);
    }

    // Преобразуем в строку
    char *json_string = cJSON_PrintUnformatted(root);

    // Очищаем JSON объект (обязательно!)
    cJSON_Delete(root);

    if (json_string == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON");
    }

    return json_string;
}

/**
 * @brief Определение поддерживаемых функций котла (один раз при старте)
 */
void um_ot_detect_supported_features(void)
{
    ESP_LOGI(TAG, "=== Detecting boiler supported features ===");

    // Быстрая проверка - есть ли котел?
    unsigned long test = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_STATUS, 0));
    vTaskDelay(pdMS_TO_TICKS(50));

    if (esp_ot_get_last_response_status() != OT_STATUS_SUCCESS)
    {
        ESP_LOGW(TAG, "Boiler not responding, skipping feature detection");
        // Устанавливаем значения по умолчанию (false)
        memset(&ot_data.supported, 0, sizeof(ot_data.supported));
        return;
    }

    unsigned long response;
    open_therm_message_type_t msg_type;

    // 1. Проверяем модуляцию (ID=17)
    response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_REL_MOD_LEVEL, 0));
    vTaskDelay(pdMS_TO_TICKS(50));
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.modulation = (msg_type != OT_UNKNOWN_DATA_ID);
    ESP_LOGI(TAG, "Modulation read: %s", ot_data.supported.modulation ? "SUPPORTED" : "NOT supported");

    // 2. Проверяем запись макс. модуляции (ID=14)
    response = esp_ot_send_request(esp_ot_build_request(OT_WRITE_DATA, MSG_ID_MAX_REL_MOD_LEVEL_SETTING, 50));
    vTaskDelay(pdMS_TO_TICKS(50));
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.modulation_write = (msg_type != OT_UNKNOWN_DATA_ID && msg_type != OT_DATA_INVALID);
    ESP_LOGI(TAG, "Modulation write: %s", ot_data.supported.modulation_write ? "SUPPORTED" : "NOT supported");

    // 3. Проверяем кривую нагрева (ID=58)
    response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_OTC_CURVE_RATIO, 0));
    vTaskDelay(pdMS_TO_TICKS(50)); // ДОБАВИТЬ
    taskYIELD();                   // ДОБАВИТЬ
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.heat_curve = (msg_type != OT_UNKNOWN_DATA_ID);
    ESP_LOGI(TAG, "Heat curve read: %s", ot_data.supported.heat_curve ? "SUPPORTED" : "NOT supported");

    // 4. Проверяем запись кривой нагрева
    response = esp_ot_send_request(esp_ot_build_request(OT_WRITE_DATA, MSG_ID_OTC_CURVE_RATIO, 50 << 8));
    vTaskDelay(pdMS_TO_TICKS(50)); // ДОБАВИТЬ
    taskYIELD();                   // ДОБАВИТЬ
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.heat_curve_write = (msg_type != OT_UNKNOWN_DATA_ID && msg_type != OT_DATA_INVALID);
    ESP_LOGI(TAG, "Heat curve write: %s", ot_data.supported.heat_curve_write ? "SUPPORTED" : "NOT supported");

    // 5. Проверяем датчик наружной температуры (ID=27)
    response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_TOUTSIDE, 0));
    vTaskDelay(pdMS_TO_TICKS(50)); // ДОБАВИТЬ
    taskYIELD();                   // ДОБАВИТЬ
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.outside_temp = (msg_type != OT_UNKNOWN_DATA_ID);

    // 6. Проверяем датчик обратки (ID=28)
    response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_TRET, 0));
    vTaskDelay(pdMS_TO_TICKS(50)); // ДОБАВИТЬ
    taskYIELD();                   // ДОБАВИТЬ
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.return_temp = (msg_type != OT_UNKNOWN_DATA_ID);

    // 7. Проверяем давление (ID=18)
    response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_CH_PRESSURE, 0));
    vTaskDelay(pdMS_TO_TICKS(50)); // ДОБАВИТЬ
    taskYIELD();                   // ДОБАВИТЬ
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.pressure = (msg_type != OT_UNKNOWN_DATA_ID);

    // 8. Проверяем расходомер DHW (ID=19)
    response = esp_ot_send_request(esp_ot_build_request(OT_READ_DATA, MSG_ID_DHW_FLOW_RATE, 0));
    vTaskDelay(pdMS_TO_TICKS(50)); // ДОБАВИТЬ
    taskYIELD();                   // ДОБАВИТЬ
    msg_type = esp_ot_get_message_type(response);
    ot_data.supported.flow_rate = (msg_type != OT_UNKNOWN_DATA_ID);

    ESP_LOGI(TAG, "=== Feature detection complete ===");
}

#endif // UM_FEATURE_ENABLED(OPENTHERM)