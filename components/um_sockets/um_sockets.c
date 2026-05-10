#include "um_sockets.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include "um_helpers.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "sockets";
static char discovered_ips[MAX_REMOTE_SERVERS][16];
static int discovered_count = 0;
static TimerHandle_t discovery_timer;

bool udp_socket_ready = false;

uint16_t socket_port = 0;

// Внутренняя функция для широковещания (fallback)
static void broadcast_fallback(const char *data)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
        return;

    int bc = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(socket_port),
        .sin_addr.s_addr = inet_addr("255.255.255.255")};

    sendto(sock, data, strlen(data), 0, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);
}

esp_err_t um_sockets_reload_discovery(void)
{
    ESP_LOGI(TAG, "Searching servers _umni_srv._udp...");

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr("_umni_srv", "_udp", 2000, MAX_REMOTE_SERVERS, &results);

    if (err != ESP_OK || !results)
    {
        discovered_count = 0;
        ESP_LOGW(TAG, "No servers found. Mode: Broadcast");
        return ESP_OK;
    }

    discovered_count = 0;
    mdns_result_t *r = results;
    while (r && discovered_count < MAX_REMOTE_SERVERS)
    {
        if (r->addr)
        {
            esp_ip4addr_ntoa(&(r->addr->addr.u_addr.ip4), discovered_ips[discovered_count], 16);
            ESP_LOGI(TAG, "Сервер найден: %s", discovered_ips[discovered_count]);
            discovered_count++;
        }
        r = r->next;
    }
    mdns_query_results_free(results);
    return ESP_OK;
}

static void discovery_timer_cb(TimerHandle_t xTimer)
{
    if (um_helpers_mdns_running())
    {
        um_sockets_reload_discovery();
    }
    else
    {
        ESP_LOGE(TAG, "um_helpers_mdns_running return false");
    }
}

esp_err_t um_sockets_init(uint16_t port)
{
    socket_port = port;
    // Гарантируем, что mDNS запущен
    esp_err_t err = um_helpers_mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    um_sockets_reload_discovery();

    // Запускаем таймер авто-обновления списка серверов
    discovery_timer = xTimerCreate("um_disc", pdMS_TO_TICKS(DISCOVERY_INTERVAL_MS), pdTRUE, NULL, discovery_timer_cb);
    xTimerStart(discovery_timer, 0);

    udp_socket_ready = true;

    return ESP_OK;
}

esp_err_t um_sockets_send(const char *data)
{
    if (!udp_socket_ready)
        return ESP_FAIL;

    if (discovered_count == 0)
    {
        broadcast_fallback(data);
        return ESP_OK;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
        return ESP_FAIL;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(socket_port)};

    size_t len = strlen(data);
    for (int i = 0; i < discovered_count; i++)
    {
        addr.sin_addr.s_addr = inet_addr(discovered_ips[i]);
        sendto(sock, data, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    }

    close(sock);
    return ESP_OK;
}

esp_err_t um_sockets_send_syslog(const char *tag, const char *json_data)
{
    if (!udp_socket_ready)
        return ESP_FAIL;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
        return ESP_FAIL;

    // 1. Формируем заголовок Syslog
    // <14> - User-level messages (Facility 1), Informational (Severity 6)
    // "umni-uni" - имя вашего устройства (можно заменить на hostname)
    static char syslog_pkt[1500];
    char *hostname = NULL;
    um_helpers_get_hostname(&hostname);
    if (hostname == NULL)
    {
        hostname = "umni-uni-unknown";
    }
    int len = snprintf(syslog_pkt, sizeof(syslog_pkt), "<14>%s %s: %s", hostname, tag, json_data);

    if (len >= sizeof(syslog_pkt))
    {
        ESP_LOGE(TAG, "Packet very big for syslog!");
        close(sock);
        return ESP_ERR_NO_MEM;
    }

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(UM_SOCKETS_PORT)};

    // 2. Рассылка по найденным серверам или Broadcast
    if (discovered_count == 0)
    {
        int bc = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bc, sizeof(bc));
        addr.sin_addr.s_addr = inet_addr("255.255.255.255");
        sendto(sock, syslog_pkt, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    }
    else
    {
        for (int i = 0; i < discovered_count; i++)
        {
            addr.sin_addr.s_addr = inet_addr(discovered_ips[i]);
            sendto(sock, syslog_pkt, len, 0, (struct sockaddr *)&addr, sizeof(addr));
        }
    }

    free(hostname);

    close(sock);
    return ESP_OK;
}

esp_err_t um_sockets_deinit(void)
{
    if (!udp_socket_ready)
        return ESP_OK;

    // 1. Останавливаем таймер
    if (discovery_timer != NULL)
    {
        xTimerStop(discovery_timer, 0);
        xTimerDelete(discovery_timer, 0);
        discovery_timer = NULL;
    }

    // 2. Сбрасываем счетчик найденных серверов
    discovered_count = 0;

    // 3. mDNS деинициализировать обычно не нужно (он общий),
    // но можно очистить конкретные запросы, если они висят.

    udp_socket_ready = false;
    ESP_LOGI(TAG, "Sockets deinitialized (Ethernet Down)");
    return ESP_OK;
}