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
     * @brief Initialize NTC store
     *
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t um_ntc_store_init(void);

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

    /**
     * @brief Получить историю измерений температуры в формате JSON для веб-интерфейса
     *
     * @details Функция возвращает все сохраненные измерения (до 20 последних) для указанного
     *          канала NTC в формате JSON, готовом для использования с ECharts или другими
     *          библиотеками визуализации. Данные возвращаются в хронологическом порядке
     *          (от старых к новым).
     *
     * @param channel_id Идентификатор канала NTC:
     *                   - UM_NTC_CHANNEL_1 (0) - первый NTC датчик
     *                   - UM_NTC_CHANNEL_2 (1) - второй NTC датчик
     *
     * @return char* Указатель на динамически выделенную строку JSON.
     *               Возвращаемые форматы:
     *               - При успехе: JSON объект с данными
     *               - При ошибке: {"error":"описание ошибки"}
     *               - При отсутствии данных: {"error":"no data"}
     *
     * @note Возвращаемую строку необходимо освободить с помощью free() после использования
     * @note Функция создает копию данных, оригинальное хранилище не модифицируется
     *
     * @par Пример возвращаемого JSON:
     * @code
     * {
     *   "name": "ntc1",                    // Имя хранилища
     *   "timestamps": [1640000000000,      // Unix timestamp в миллисекундах
     *                  1640000001000,
     *                  1640000002000],
     *   "values": [24.5, 24.6, 24.4],     // Температуры в °C
     *   "count": 3                        // Количество записей
     * }
     * @endcode
     *
     * @par Пример использования:
     * @code
     * // В обработчике HTTP запроса
     * char *json_data = um_ntc_get_history_json(UM_NTC_CHANNEL_1);
     * if (json_data) {
     *     // Отправляем JSON клиенту
     *     httpd_resp_set_type(req, "application/json");
     *     httpd_resp_send(req, json_data, strlen(json_data));
     *     free(json_data);
     * }
     * @endcode
     *
     * @par Использование с ECharts на фронтенде:
     * @code
     * // JavaScript код
     * fetch('/api/ntc/history?channel=0')
     *   .then(response => response.json())
     *   .then(data => {
     *       // Преобразуем для ECharts
     *       const chartData = data.timestamps.map((ts, i) => [ts, data.values[i]]);
     *       myChart.setOption({
     *           xAxis: { type: 'time' },
     *           series: [{ data: chartData, type: 'line' }]
     *       });
     *   });
     * @endcode
     *
     * @warning Функция выделяет память под JSON строку. При большой истории (20 записей)
     *          JSON весит примерно 500-600 байт. Убедитесь, что у вас достаточно heap памяти.
     *
     * @see um_ntc_get_stats()
     * @see um_store_to_json()
     */
    char *um_ntc_get_history_json(um_ntc_channel_id_t channel_id);

    /**
     * @brief Получить статистическую сводку по измерениям температуры
     *
     * @details Функция вычисляет основные статистические показатели для указанного канала
     *          на основе всех сохраненных измерений (до 20 последних). Это полезно для
     *          мониторинга и диагностики системы.
     *
     * @param channel_id Идентификатор канала NTC:
     *                   - UM_NTC_CHANNEL_1 (0) - первый NTC датчик
     *                   - UM_NTC_CHANNEL_2 (1) - второй NTC датчик
     *
     * @param min [out] Указатель на переменную для минимальной температуры (°C)
     *                  Может быть NULL, если значение не нужно
     *
     * @param max [out] Указатель на переменную для максимальной температуры (°C)
     *                  Может быть NULL, если значение не нужно
     *
     * @param avg [out] Указатель на переменную для средней температуры (°C)
     *                  Может быть NULL, если значение не нужно
     *
     * @note Если в хранилище нет данных, все выходные параметры устанавливаются в 0
     * @note Функция не выделяет память и не блокирует выполнение программы
     *
     * @par Пример использования 1 - получить все показатели:
     * @code
     * float min_temp, max_temp, avg_temp;
     * um_ntc_get_stats(UM_NTC_CHANNEL_1, &min_temp, &max_temp, &avg_temp);
     *
     * ESP_LOGI("NTC", "Channel 1 stats: min=%.1f°C, max=%.1f°C, avg=%.1f°C",
     *          min_temp, max_temp, avg_temp);
     * // Вывод: Channel 1 stats: min=22.5°C, max=25.3°C, avg=23.8°C
     * @endcode
     *
     * @par Пример использования 2 - получить только среднее:
     * @code
     * float average;
     * um_ntc_get_stats(UM_NTC_CHANNEL_2, NULL, NULL, &average);
     *
     * if (average > 30.0f) {
     *     ESP_LOGW("NTC", "High temperature detected! Average: %.1f°C", average);
     *     // Включить вентилятор или отправить предупреждение
     * }
     * @endcode
     *
     * @par Пример использования 3 - проверка стабильности:
     * @code
     * float min_t, max_t;
     * um_ntc_get_stats(UM_NTC_CHANNEL_1, &min_t, &max_t, NULL);
     *
     * float variance = max_t - min_t;
     * if (variance > 5.0f) {
     *     ESP_LOGW("NTC", "Temperature unstable! Variance: %.1f°C", variance);
     *     // Сигнал о нестабильности процесса
     * }
     * @endcode
     *
     * @par Пример использования 4 - HTTP API:
     * @code
     * // Создаем JSON ответ для REST API
     * float min_t, max_t, avg_t;
     * um_ntc_get_stats(UM_NTC_CHANNEL_1, &min_t, &max_t, &avg_t);
     *
     * cJSON *root = cJSON_CreateObject();
     * cJSON_AddNumberToObject(root, "min", min_t);
     * cJSON_AddNumberToObject(root, "max", max_t);
     * cJSON_AddNumberToObject(root, "avg", avg_t);
     * cJSON_AddNumberToObject(root, "samples", 20);
     *
     * char *response = cJSON_Print(root);
     * httpd_resp_send(req, response, strlen(response));
     * free(response);
     * cJSON_Delete(root);
     * @endcode
     *
     * @par Пример использования 5 - Автоматическая калибровка:
     * @code
     * // Если среднее отклоняется от нормы, корректируем
     * float avg_temp;
     * um_ntc_get_stats(UM_NTC_CHANNEL_1, NULL, NULL, &avg_temp);
     *
     * static float baseline = 25.0f;
     * if (abs(avg_temp - baseline) > 5.0f) {
     *     // Возможно, датчик дрейфует или изменились условия
     *     ESP_LOGW("NTC", "Temperature drift detected: baseline=%.1f, avg=%.1f",
     *              baseline, avg_temp);
     *     // Отправить уведомление оператору
     * }
     * @endcode
     *
     * @note Функция потокобезопасна при условии, что хранилище не модифицируется
     *       во время выполнения (чтение атомарно)
     *
     * @warning Для пустого хранилища все выходные параметры будут равны 0, что
     *          может быть интерпретировано как реальные показания (0°C). Всегда
     *          проверяйте количество записей через um_ntc_get_history_json или
     *          храните отдельно счетчик.
     *
     * @see um_ntc_get_history_json()
     * @see um_store_stats()
     */
    void um_ntc_get_stats(um_ntc_channel_id_t channel_id, float *min, float *max, float *avg);

#ifdef __cplusplus
}
#endif

#endif // UM_NTC_H