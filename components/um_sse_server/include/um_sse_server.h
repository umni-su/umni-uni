#ifndef UM_SSE_SERVER_H
#define UM_SSE_SERVER_H

#include <esp_http_server.h>

/**
 * @brief Инициализация SSE сервера
 * @param server Дескриптор запущенного http_server
 * @param uri Путь, например "/events"
 */
void um_sse_server_init(httpd_handle_t server, const char *uri);

void um_sse_server_deinit(void);

/**
 * @brief Отправка события всем подключенным клиентам
 * @param event_name Имя события (например, "update")
 * @param data Данные в формате строки (обычно JSON)
 */
esp_err_t um_sse_publish_event(const char *event_name, const char *data);

#endif