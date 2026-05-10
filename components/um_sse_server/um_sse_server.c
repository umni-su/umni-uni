#include "um_sse_server.h"
#include <esp_log.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define MAX_CLIENTS (CONFIG_LWIP_MAX_SOCKETS - 3)
#define MAX_SSE_CLIENTS 4

static const char *TAG = "UM_SSE_SERVER";
static httpd_handle_t _server = NULL;
static int client_fds[MAX_CLIENTS];
static TimerHandle_t sse_ping_timer;
static bool sse_initialized = false;

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
            if (send_chunk_raw(client_fds[i], ping_msg, strlen(ping_msg)) != ESP_OK)
            {
                ESP_LOGD(TAG, "Client FD %d removed on ping fail", client_fds[i]);
                client_fds[i] = -1;
            }
        }
    }
}

// Обработчик подключения (SSE Handler)
static esp_err_t sse_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    int fd = httpd_req_to_sockfd(req);
    int current_sse_count = 0;
    int slot_idx = -1;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_fds[i] != -1)
            current_sse_count++;
        if (client_fds[i] == -1 && slot_idx == -1)
            slot_idx = i;
    }

    if (current_sse_count >= MAX_SSE_CLIENTS || slot_idx == -1)
    {
        ESP_LOGW(TAG, "SSE limit reached, rejecting FD: %d", fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Max clients reached");
        return ESP_FAIL;
    }

    client_fds[slot_idx] = fd;
    ESP_LOGI(TAG, "New SSE client connected: FD %d", fd);

    // ВАЖНО: Первый ответ отправляем через стандартную функцию,
    // чтобы сервер правильно выставил заголовки Chunked Transfer
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
    sse_ping_timer = xTimerCreate("sse_ping", pdMS_TO_TICKS(10000), pdTRUE, NULL, sse_ping_timer_cb);
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
            if (send_chunk_raw(client_fds[i], payload, len) != ESP_OK)
            {
                ESP_LOGD(TAG, "Client FD %d disconnected", client_fds[i]);
                client_fds[i] = -1;
            }
        }
    }
    return ESP_OK;
}