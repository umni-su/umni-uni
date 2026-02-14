#pragma once

#include "esp_err.h"
#include "base_config.h"
#include "esp_http_server.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)

    static esp_err_t um_webserver_base_get_handler(
        httpd_req_t *req,
        esp_err_t (*get_data)(httpd_req_t *, cJSON **));

    static esp_err_t um_webserver_base_post_handler(
        httpd_req_t *req,
        esp_err_t (*process_data)(httpd_req_t *, cJSON *input, cJSON **output));

    esp_err_t um_webserver_register_get(const char *uri, esp_err_t (*handler)(httpd_req_t *, cJSON **));

    /**
     * @brief Зарегистрировать POST endpoint
     * @param uri URI endpoint (например "/api/data")
     * @param handler функция обработки (req, input_json, output_json)
     * @return esp_err_t
     */
    esp_err_t um_webserver_register_post(const char *uri,
                                         esp_err_t (*process_func)(httpd_req_t *, cJSON *, cJSON **));

    /**
     * @brief Инициализация и запуск веб-сервера
     *
     * @return esp_err_t
     */
    esp_err_t um_webserver_start(void);

    /**
     * @brief Остановка веб-сервера
     *
     * @return esp_err_t
     */
    esp_err_t um_webserver_stop(void);

#endif // UM_FEATURE_ENABLED(WEBSERVER)

#ifdef __cplusplus
}
#endif