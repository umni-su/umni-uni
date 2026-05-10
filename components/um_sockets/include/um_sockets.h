#ifndef UM_SOCKETS_H
#define UM_SOCKETS_H

#include "esp_err.h"

#define UM_SOCKETS_PORT 514          // Порт для UDP вещания
#define MAX_REMOTE_SERVERS 5         // Макс. кол-во найденных серверов
#define DISCOVERY_INTERVAL_MS 300000 // Интервал перепоиска (5 минут)

/**
 * @brief Инициализация сокетов и запуск фонового поиска серверов
 */
esp_err_t um_sockets_init(uint16_t port);

/**
 * @brief Деинициализация сокетов
 */
esp_err_t um_sockets_deinit(void);

/**
 * @brief Принудительное обновление списка серверов через mDNS
 */
esp_err_t um_sockets_reload_discovery(void);

/**
 * @brief Универсальный метод отправки данных.
 * Сначала шлет на найденные IP, если их нет — делает Broadcast.
 */
esp_err_t um_sockets_send(const char *data);

/**
 * @brief Универсальный метод отправки данных в формате  Syslog (RFC 5424 / RFC 3164).
 * Сначала шлет на найденные IP, если их нет — делает Broadcast.
 */
esp_err_t um_sockets_send_syslog(const char *tag, const char *json_data);

#endif