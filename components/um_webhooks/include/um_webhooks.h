#ifndef UM_WEBHOOKS_H
#define UM_WEBHOOKS_H

#include "base_config.h"
#include "um_nvs.h"
#include "esp_http_client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if UM_FEATURE_ENABLED(WEBHOOKS)

    /**
     * @brief Проверить, включены ли вебхуки
     *
     * @return true - вебхуки включены, false - выключены или ошибка
     */
    bool um_webhooks_is_enabled(void);

    /**
     * @brief Получить URL вебхука
     *
     * @return char* URL вебхука (нужно освободить через free()), или NULL если не задан
     */
    char *um_webhooks_get_url(void);

    /**
     * @brief Отправить POST запрос с JSON строкой
     *
     * @param json_string JSON строка для отправки (может быть NULL)
     * @return esp_err_t ESP_OK при успехе, иначе ошибка
     */
    esp_err_t um_webhooks_post_string(const char *json_string);

    /**
     * @brief Отправить POST запрос с cJSON объектом
     *
     * @param json cJSON объект для отправки (может быть NULL)
     * @return esp_err_t ESP_OK при успехе, иначе ошибка
     */
    esp_err_t um_webhooks_post_json(cJSON *json);

    /**
     * @brief Отправить GET запрос (параметры в URL)
     *
     * @param query_string строка запроса (например "?param1=value1&param2=value2")
     * @return esp_err_t ESP_OK при успехе, иначе ошибка
     */
    esp_err_t um_webhooks_get(const char *query_string);

    /**
     * @brief Отправить GET запрос  (параметры в URL)
     *
     * @param query_string строка запроса (например "?param1=value1&param2=value2")
     * @param timeout_ms таймаут в миллисекундах (0 - использовать умолчание)
     * @return esp_err_t ESP_OK при успехе, иначе ошибка
     */
    esp_err_t um_webhooks_get_timeout(const char *query_string, int timeout_ms);

    /**
     * @brief Отправить POST запрос с JSON строкой (с таймаутом)
     *
     * @param json_string JSON строка для отправки
     * @param timeout_ms таймаут в миллисекундах (0 - использовать умолчание)
     * @return esp_err_t ESP_OK при успехе, иначе ошибка
     */
    esp_err_t um_webhooks_post_string_timeout(const char *json_string, int timeout_ms);

    /**
     * @brief Отправить POST запрос с cJSON объектом (с таймаутом)
     *
     * @param json cJSON объект для отправки
     * @param timeout_ms таймаут в миллисекундах (0 - использовать умолчание)
     * @return esp_err_t ESP_OK при успехе, иначе ошибка
     */
    esp_err_t um_webhooks_post_json_timeout(cJSON *json, int timeout_ms);

#endif

#ifdef __cplusplus
}
#endif

#endif