#ifndef UM_SOCKETS_H
#define UM_SOCKETS_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define UM_SOCKETS_PORT 514          // Порт для UDP вещания
#define MAX_REMOTE_SERVERS 5         // Макс. кол-во найденных серверов
#define DISCOVERY_INTERVAL_MS 300000 // Интервал перепоиска (5 минут)

// mDNS-сервис, который публикует каждое устройство (см. um_helpers)
#define UM_SOCKETS_MDNS_SERVICE "_umni_api"
#define UM_SOCKETS_MDNS_PROTO "_tcp"

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

/**
 * @brief Разрешить device_id в IP/порт через mDNS (_umni_api._tcp).
 *
 * Ищет сервис, у которого TXT-запись (id/unique_id/hostname) или instance_name
 * совпадает с device_id. Нужно для обращения к peer'ам при смене IP.
 *
 * @param device_id Идентификатор устройства (hostname / unique_id)
 * @param[out] ip_out Буфер под IPv4-строку
 * @param ip_len Размер буфера (минимум 16)
 * @param[out] port_out Порт сервиса (может быть NULL)
 * @return ESP_OK если найден, ESP_ERR_NOT_FOUND / ESP_ERR_INVALID_STATE иначе
 */
esp_err_t um_sockets_resolve(const char *device_id,
                             char *ip_out, size_t ip_len,
                             uint16_t *port_out);

#endif