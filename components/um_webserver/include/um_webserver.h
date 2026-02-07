#pragma once

#include "esp_err.h"
#include "base_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)

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