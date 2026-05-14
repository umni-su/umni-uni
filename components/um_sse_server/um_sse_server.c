#include "um_sse_server.h"
#include "esp_log.h"
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <lwip/sockets.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define MAX_CLIENTS (CONFIG_LWIP_MAX_SOCKETS - 3)
#define MAX_SSE_CLIENTS 4

static const char *TAG = "UM_SSE_SERVER";
static httpd_handle_t _server = NULL;
static int client_fds[MAX_CLIENTS];
static TimerHandle_t sse_ping_timer;
static bool sse_initialized = false;

static bool is_socket_alive(int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLERR | POLLHUP;
    int ret = poll(&pfd, 1, 0);
    return !(ret < 0 || (pfd.revents & (POLLERR | POLLHUP)));
}

// Вспомогательная функция для отправки данных в формате HTTP Chunk
// [HEX_LEN]\r\n[DATA]\r\n
static esp_err_t send_chunk_raw(int fd, const char *data, size_t len)
{
    char header[16];
    int header_len = snprintf(header, sizeof(header), "%X\r\n", (unsigned int)len);

    // 1. Отправляем длину
    if (httpd_socket_send(_server, fd, header, header_len, 0) < 0)
        return ESP_FAIL;
    // 2. Отправляем данные
    if (httpd_socket_send(_server, fd, data, len, 0) < 0)
        return ESP_FAIL;
    // 3. Отправляем хвост чанка
    if (httpd_socket_send(_server, fd, "\r\n", 2, 0) < 0)
        return ESP_FAIL;

    return ESP_OK;
}

// Таймер для поддержания соединения (Keep-alive)
static void sse_ping_timer_cb(TimerHandle_t xTimer)
{
    if (!_server)
        return;
    const char *ping_msg = ": ping\n\n";
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_fds[i] != -1)
        {
            // Сначала проверяем, жив ли сокет
            if (!is_socket_alive(client_fds[i]))
            {
                ESP_LOGI(TAG, "Client FD %d dead (poll), removing", client_fds[i]);
                client_fds[i] = -1;
                continue;
            }

            // Потом пробуем отправить ping
            if (send_chunk_raw(client_fds[i], ping_msg, strlen(ping_msg)) != ESP_OK)
            {
                ESP_LOGI(TAG, "Client FD %d removed on ping fail", client_fds[i]);
                client_fds[i] = -1;
            }
        }
    }
}

static void get_client_ip(int fd, char *ip_str, size_t ip_str_len)
{
    struct sockaddr_in6 addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == 0)
    {
        // Проверяем, является ли адрес IPv4-mapped IPv6
        // IPv4-mapped IPv6 имеет вид ::ffff:192.168.1.100
        const uint32_t *addr32 = (uint32_t *)&addr.sin6_addr;
        if (addr32[0] == 0 && addr32[1] == 0 &&
            ntohl(addr32[2]) == 0xFFFF)
        {
            // Это IPv4-mapped IPv6 адрес, извлекаем IPv4 часть
            uint32_t ipv4_addr = ntohl(addr32[3]);
            snprintf(ip_str, ip_str_len, "%d.%d.%d.%d",
                     (int)((ipv4_addr >> 24) & 0xFF),
                     (int)((ipv4_addr >> 16) & 0xFF),
                     (int)((ipv4_addr >> 8) & 0xFF),
                     (int)(ipv4_addr & 0xFF));
        }
        else
        {
            // Настоящий IPv6 адрес
            inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, ip_str_len);
        }
    }
    else
    {
        strcpy(ip_str, "unknown");
    }
}

// Обработчик подключения (SSE Handler)
static esp_err_t sse_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    int fd = httpd_req_to_sockfd(req);

    // Получаем IP нового клиента (только для логов и сравнения)
    char client_ip[16];
    get_client_ip(fd, client_ip, sizeof(client_ip));
    ESP_LOGI(TAG, "New connection from IP: %s", client_ip);

    // Получаем IPv4 для сравнения (числовое значение)
    struct sockaddr_in6 client_addr6;
    socklen_t addr_len = sizeof(client_addr6);
    uint32_t client_ipv4 = 0;

    if (getpeername(fd, (struct sockaddr *)&client_addr6, &addr_len) == 0)
    {
        const uint32_t *addr32 = (uint32_t *)&client_addr6.sin6_addr;
        client_ipv4 = ntohl(addr32[3]); // Для IPv4-mapped
    }

    // Удаляем старые сокеты с того же IP
    if (client_ipv4 != 0)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (client_fds[i] != -1)
            {
                struct sockaddr_in6 old_addr6;
                socklen_t old_len = sizeof(old_addr6);
                if (getpeername(client_fds[i], (struct sockaddr *)&old_addr6, &old_len) == 0)
                {
                    const uint32_t *old_addr32 = (uint32_t *)&old_addr6.sin6_addr;
                    uint32_t old_ipv4 = ntohl(old_addr32[3]);

                    if (client_ipv4 == old_ipv4)
                    {
                        ESP_LOGI(TAG, "Removing old socket FD %d from same IP %s",
                                 client_fds[i], client_ip);
                        client_fds[i] = -1;
                    }
                }
            }
        }
    }

    // Считаем актуальное количество клиентов (ПОСЛЕ очистки!)
    int current_sse_count = 0;
    int slot_idx = -1;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_fds[i] != -1)
        {
            current_sse_count++;
        }
        else if (slot_idx == -1)
        {
            slot_idx = i;
        }
    }

    // Проверяем лимит
    if (current_sse_count >= MAX_SSE_CLIENTS)
    {
        ESP_LOGW(TAG, "SSE limit reached (%d/%d), rejecting FD: %d",
                 current_sse_count, MAX_SSE_CLIENTS, fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Max clients reached");
        return ESP_FAIL;
    }

    if (slot_idx == -1)
    {
        ESP_LOGW(TAG, "No free slots, rejecting FD: %d", fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No free slots");
        return ESP_FAIL;
    }

    // Добавляем нового клиента
    client_fds[slot_idx] = fd;
    ESP_LOGI(TAG, "New SSE client connected: FD %d (slot %d, active: %d/%d)",
             fd, slot_idx, current_sse_count + 1, MAX_SSE_CLIENTS);

    // Отправляем первый чанк
    const char *start_msg = ": ok\n\n";
    return httpd_resp_send_chunk(req, start_msg, strlen(start_msg));
}

void um_sse_server_init(httpd_handle_t server, const char *uri)
{
    _server = server;
    for (int i = 0; i < MAX_CLIENTS; i++)
        client_fds[i] = -1;

    httpd_uri_t sse_uri = {
        .uri = uri,
        .method = HTTP_GET,
        .handler = sse_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &sse_uri);

    // Таймер пинга раз в 10 секунд (чуть меньше вашего таймаута в 15с)
    sse_ping_timer = xTimerCreate("sse_ping", pdMS_TO_TICKS(5000), pdTRUE, NULL, sse_ping_timer_cb);
    xTimerStart(sse_ping_timer, 0);
    sse_initialized = true;
}

esp_err_t um_sse_publish_event(const char *event_name, const char *data)
{
    if (!_server || !sse_initialized)
        return ESP_FAIL;

    // Резервируем буфер под событие (увеличьте, если JSON очень большой)
    char payload[1024];
    int len = snprintf(payload, sizeof(payload), "event: %s\ndata: %s\n\n", event_name, data);

    if (len >= sizeof(payload))
    {
        ESP_LOGE(TAG, "Event data too big!");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_fds[i] != -1)
        {
            if (!is_socket_alive(client_fds[i]))
            {
                char dead_ip[16];
                get_client_ip(client_fds[i], dead_ip, sizeof(dead_ip));
                ESP_LOGI(TAG, "Client FD %d (IP: %s) dead, removing", client_fds[i], dead_ip);
                client_fds[i] = -1;
                continue;
            }

            if (send_chunk_raw(client_fds[i], payload, len) != ESP_OK)
            {
                ESP_LOGD(TAG, "Client FD %d disconnected", client_fds[i]);
                client_fds[i] = -1;
            }
        }
    }
    return ESP_OK;
}