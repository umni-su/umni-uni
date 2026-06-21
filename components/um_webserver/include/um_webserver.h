#pragma once
#include <fcntl.h>  // для open() и O_RDONLY
#include <unistd.h> // для read(), close()
#include "esp_err.h"
#include "base_config.h"
#include "esp_http_server.h"
#include "cJSON.h"

#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#if UM_FEATURE_ENABLED(SDCARD)
#include "um_sd.h"
#define FILE_PATH_MAX (UM_SD_VFS_PATH_MAX + 255) // CONFIG_FATFS_MAX_LFN not work?
#else
#define FILE_PATH_MAX 0
#endif

#define SCRATCH_BUFSIZE (8192)
#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

#ifdef __cplusplus
extern "C"
{
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)

    esp_err_t um_webserver_base_get_handler(
        httpd_req_t *req,
        esp_err_t (*get_data)(httpd_req_t *, cJSON **));

    esp_err_t um_webserver_base_post_handler(
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

    esp_err_t um_webserver_mark_ota(void);

#endif // UM_FEATURE_ENABLED(WEBSERVER)

#ifdef __cplusplus
}
#endif