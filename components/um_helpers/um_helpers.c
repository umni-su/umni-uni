#include <string.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "lwip/ip_addr.h"
#include "um_helpers.h"
#include "um_nvs.h"
#include "esp_sntp.h"
#include "time.h"
#include "um_capabilities.h"
#include "mdns.h"

#define DEVICE_NAME_PREFIX "umni-"
#define DEVICE_NAME_MAX_LEN 32

static const char *TAG = "helpers";
static const char *MDNS_TAG = "mdns";

bool s_time_synced = false;
bool s_mdns_init = false;

// Инициализация SNTP
void um_helpers_time_init(void)
{
    if (s_time_synced)
        return;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    char *ntp_server = NULL;
    um_nvs_get_ntp(&ntp_server);
    if (ntp_server != NULL)
    {
        ESP_LOGI(TAG, "Initializing SNTP %s...", ntp_server);
        esp_sntp_setservername(0, ntp_server);
    }
    else
    {
        ESP_LOGI(TAG, "Initializing SNTP with default server");
        esp_sntp_setservername(0, "0.ru.pool.ntp.org");
    }

    esp_sntp_setservername(2, "1.ru.pool.ntp.org");
    esp_sntp_setservername(2, "2.ru.pool.ntp.org");

    esp_sntp_init();

    // Ждем синхронизации
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int max_retry = 10;

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < max_retry)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, max_retry);
        vTaskDelay(pdMS_TO_TICKS(2000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry < max_retry)
    {
        s_time_synced = true;
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "Time synchronized: %s", time_str);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to sync time, using uptime (relative timestamps)");
        s_time_synced = true; // TODO false and normal resync task
    }
}

// Функция получения реального timestamp в миллисекундах
uint64_t um_helpers_get_real_timestamp_ms(void)
{
    if (s_time_synced)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
    }
    else
    {
        // Fallback на относительное время
        return esp_timer_get_time() / 1000;
    }
}

// Проверка синхронизации времени
bool um_helpers_is_time_synced(void)
{
    return s_time_synced;
}

/**
 * @brief Получает информацию о всех сетевых интерфейсах
 */
int um_helpers_get_network_interfaces(um_network_interface_info_t *interfaces, int max_count)
{
    if (!interfaces || max_count <= 0)
    {
        return 0;
    }

    int count = 0;
    esp_netif_t *netif = esp_netif_next_unsafe(NULL);

    while (netif != NULL && count < max_count)
    {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            um_network_interface_info_t *info = &interfaces[count];
            const char *ifkey = esp_netif_get_ifkey(netif);
            const char *desc = esp_netif_get_desc(netif);

            // Красиво именуем интерфейс для вывода
            if (strstr(ifkey, "STA"))
            {
                strncpy(info->interface_name, "wifi_sta", sizeof(info->interface_name));
            }
            else if (strstr(ifkey, "AP"))
            {
                strncpy(info->interface_name, "wifi_ap", sizeof(info->interface_name));
            }
            else if (strstr(ifkey, "ETH"))
            {
                strncpy(info->interface_name, "ethernet", sizeof(info->interface_name));
            }
            else
            {
                // Если ключ специфичный (как у W5500), копируем как есть
                strncpy(info->interface_name, ifkey, sizeof(info->interface_name));
            }

            esp_err_t mac_ret = ESP_FAIL;
            uint8_t mac[6] = {0};

            // Пытаемся получить MAC по типу интерфейса
            if (strstr(ifkey, "STA") || strstr(ifkey, "AP"))
            {
                // Для WiFi интерфейсов
                if (strstr(ifkey, "STA"))
                {
                    mac_ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
                }
                else if (strstr(ifkey, "AP"))
                {
                    mac_ret = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
                }
            }
            else if (strstr(ifkey, "ETH") || (desc && strstr(desc, "eth")))
            {
                // Для Ethernet интерфейсов
                mac_ret = esp_read_mac(mac, ESP_MAC_ETH);
            }

            // Если не удалось получить MAC по типу, пробуем получить из netif
            if (mac_ret != ESP_OK)
            {
                // В ESP-IDF 5.5.2 можно попробовать получить MAC напрямую из netif
                // Это зависит от конкретной реализации интерфейса
                esp_netif_get_mac(netif, mac); // Некоторые версии имеют такую функцию
            }

            // Заполняем сетевые параметры
            snprintf(info->ip_address, sizeof(info->ip_address), IPSTR, IP2STR(&ip_info.ip));
            snprintf(info->netmask, sizeof(info->netmask), IPSTR, IP2STR(&ip_info.netmask));
            snprintf(info->gateway, sizeof(info->gateway), IPSTR, IP2STR(&ip_info.gw));
            snprintf(info->mac_address, sizeof(info->mac_address),
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            // Интерфейс считаем активным, если поднят линк и получен IP
            // Дополнительно можно проверить esp_netif_is_netif_up(netif)
            info->is_active = (ip_info.ip.addr != 0) ? 1 : 0;

            count++;
        }
        netif = esp_netif_next_unsafe(netif);
    }

    return count;
}

/**
 * @brief Получает информацию о доступной памяти
 */
int um_helpers_get_memory_info(um_memory_info_t *info)
{
    if (!info)
    {
        return -1;
    }

    // Информация о внутренней памяти
    info->total_heap = esp_get_free_heap_size() + esp_get_free_internal_heap_size();
    info->free_heap = esp_get_free_heap_size();
    info->min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);

    return 0;
}

/**
 * @brief Генерирует имя устройства на основе MAC-адреса
 * @param prefix Префикс имени (например "umni-")
 * @param buffer Буфер для записи имени
 * @param buffer_size Размер буфера
 * @return char* Указатель на buffer или NULL при ошибке
 */
char *um_helpers_generate_device_name_from_mac(const char *prefix, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 16)
    {
        return NULL;
    }

    uint8_t mac[6];

    // Получаем MAC-адрес
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK)
    {
        ret = esp_read_mac(mac, ESP_MAC_ETH);
        if (ret != ESP_OK)
        {
            ESP_LOGE("DEVICE_NAME", "Failed to read MAC address");
            return NULL;
        }
    }

    // Используем последние 3 байта MAC для уникальности
    const char *default_prefix = prefix ? prefix : "device-";

    snprintf(buffer, buffer_size, "%s%02x%02x%02x",
             default_prefix, mac[3], mac[4], mac[5]);

    return buffer;
}

/**
 * @brief Альтернативный вариант с полным MAC-адресом
 */
char *um_helpers_generate_device_name_full_mac(const char *prefix, char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 24)
    {
        return NULL;
    }

    uint8_t mac[6];

    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK)
    {
        ret = esp_read_mac(mac, ESP_MAC_ETH);
        if (ret != ESP_OK)
        {
            ESP_LOGE("DEVICE_NAME", "Failed to read MAC address");
            return NULL;
        }
    }

    const char *default_prefix = prefix ? prefix : "device-";

    snprintf(buffer, buffer_size, "%s%02x%02x%02x%02x%02x%02x",
             default_prefix, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return buffer;
}

esp_err_t um_helpers_get_systeminfo(cJSON **data)
{
    cJSON *systeminfo = cJSON_CreateObject();
    if (!systeminfo)
        return ESP_ERR_NO_MEM;
    cJSON *networks = cJSON_CreateArray();
    cJSON *heap = cJSON_CreateObject();

    char *hostname = NULL;
    if (um_nvs_get_hostname(&hostname) == ESP_OK && hostname)
    {
        cJSON_AddStringToObject(systeminfo, "hostname", hostname);
        free(hostname);
    }
    else
    {
        cJSON_AddStringToObject(systeminfo, "hostname", "unknown");
    }

    cJSON_AddStringToObject(systeminfo, "fw_ver", CONFIG_UMNI_FW_VERSION);
    cJSON_AddNumberToObject(systeminfo, "uptime", esp_timer_get_time());
    cJSON_AddNumberToObject(systeminfo, "reset_reason", (int)esp_reset_reason());

    char *cap_json = um_capabilities_get_json_array();
    if (cap_json)
    {
        cJSON *capabilities = cJSON_Parse(cap_json);
        if (capabilities)
        {
            if (cJSON_IsArray(capabilities))
            {
                // capabilities переходит под управление systeminfo
                cJSON_AddItemToObject(systeminfo, "capabilities", capabilities);
            }
            else
            {
                // Если не массив - удаляем
                cJSON_Delete(capabilities);
            }
        }
        free(cap_json);
    }

    um_network_interface_info_t interfaces[3];
    int count = um_helpers_get_network_interfaces(interfaces, 3);

    for (int i = 0; i < count; i++)
    {
        cJSON *network_item = cJSON_CreateObject();
        cJSON_AddStringToObject(network_item, "name", interfaces[i].interface_name);
        cJSON_AddStringToObject(network_item, "mac", interfaces[i].mac_address);
        cJSON_AddStringToObject(network_item, "ip", interfaces[i].ip_address);
        cJSON_AddStringToObject(network_item, "mask", interfaces[i].netmask);
        cJSON_AddStringToObject(network_item, "gw", interfaces[i].gateway);
        cJSON_AddBoolToObject(network_item, "active", interfaces[i].is_active);
        cJSON_AddItemToArray(networks, network_item);
    }

    // Получаем информацию о памяти
    um_memory_info_t mem_info;
    if (um_helpers_get_memory_info(&mem_info) == 0)
    {
        cJSON_AddNumberToObject(heap, "total", mem_info.total_heap);
        cJSON_AddNumberToObject(heap, "free", mem_info.free_heap);
        cJSON_AddNumberToObject(heap, "min", mem_info.min_free_heap);
    }

    cJSON_AddItemToObject(systeminfo, "networks", networks);
    cJSON_AddItemToObject(systeminfo, "heap", heap);

    *data = systeminfo;

    return ESP_OK;
}

esp_err_t um_helpers_get_hostname(char **hostname)
{
    return um_nvs_get_hostname(hostname);
}

esp_err_t um_helpers_mdns_init(void)
{
    if (s_mdns_init)
        return ESP_OK;
    ESP_LOGI(MDNS_TAG, "Initializing mDNS...");

    // Инициализируем mDNS
    esp_err_t err = mdns_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(MDNS_TAG, "mDNS init failed: %d", err);
        return err;
    }

    // Получаем hostname из NVS (как в вашем коде)
    char *hostname = NULL;
    err = um_nvs_get_hostname(&hostname);

    if (err == ESP_OK && hostname != NULL && strlen(hostname) > 0)
    {
        // Используем сохранённый hostname
        mdns_hostname_set(hostname);
        ESP_LOGI(MDNS_TAG, "Hostname set from NVS: %s.local", hostname);
        free(hostname);
    }
    else
    {
        // Генерируем из MAC как fallback
        char generated_name[DEVICE_NAME_MAX_LEN];
        um_helpers_generate_device_name_from_mac(DEVICE_NAME_PREFIX,
                                                 generated_name,
                                                 sizeof(generated_name));
        mdns_hostname_set(generated_name);
        ESP_LOGI(MDNS_TAG, "Hostname generated from MAC: %s.local", generated_name);
    }

    // Устанавливаем инстанс имя (friendly name)
    mdns_instance_name_set("UMNI Smart Controller");

    err = um_mdns_add_basic_services();
    err = um_mdns_add_discovery();

    ESP_LOGI(MDNS_TAG, "mDNS initialized successfully");
    s_mdns_init = true;
    return ESP_OK;
}

bool um_helpers_mdns_running(void)
{
    return s_mdns_init;
}

// Добавление базовых сервисов для обнаружения
esp_err_t um_mdns_add_basic_services(void)
{
    ESP_LOGI(MDNS_TAG, "Adding basic mDNS services...");

    // 1. HTTP сервис (для веб-интерфейса)
    mdns_txt_item_t http_txt[] = {
        {"path", "/api/"},
        {"api_version", "1.0"}};

    esp_err_t err = mdns_service_add("UMNI Web Interface",
                                     "_http", "_tcp", 80,
                                     http_txt,
                                     sizeof(http_txt) / sizeof(http_txt[0]));
    if (err != ESP_OK)
    {
        ESP_LOGW(MDNS_TAG, "Failed to add HTTP service: %d", err);
    }

    // 2. Home Assistant API (для автоматического обнаружения)
    mdns_txt_item_t ha_txt[] = {
        {"api", "true"},
        {"version", CONFIG_UMNI_FW_VERSION},
        {"device_class", "controller"}};

    err = mdns_service_add("UMNI HA Integration",
                           "_homeassistant", "_tcp", 8123,
                           ha_txt,
                           sizeof(ha_txt) / sizeof(ha_txt[0]));
    if (err != ESP_OK)
    {
        ESP_LOGW(MDNS_TAG, "Failed to add HA service: %d", err);
    }

    ESP_LOGI(MDNS_TAG, "Basic services added");
    return ESP_OK;
}

// Универсальная функция добавления кастомного сервиса
esp_err_t um_mdns_add_service(const char *instance_name,
                              const char *service_type,
                              const char *proto,
                              uint16_t port,
                              um_mdns_txt_item_t *txt_items,
                              size_t txt_count)
{
    if (!service_type || !proto)
    {
        ESP_LOGE(MDNS_TAG, "Invalid parameters: service_type and proto required");
        return ESP_ERR_INVALID_ARG;
    }

    // Используем instance_name или hostname если не указан
    char *hostname = NULL;
    um_nvs_get_hostname(&hostname);

    if (!instance_name)
    {
        instance_name = hostname ? hostname : "UMNI Device";
    }

    // Конвертируем наш формат TXT в формат mdns
    mdns_txt_item_t *mdns_txt = NULL;
    if (txt_items && txt_count > 0)
    {
        mdns_txt = (mdns_txt_item_t *)malloc(sizeof(mdns_txt_item_t) * txt_count);
        if (!mdns_txt)
        {
            ESP_LOGE(MDNS_TAG, "Memory allocation failed");
            if (hostname)
                free(hostname);
            return ESP_ERR_NO_MEM;
        }

        for (size_t i = 0; i < txt_count; i++)
        {
            mdns_txt[i].key = txt_items[i].key;
            mdns_txt[i].value = txt_items[i].value;
        }
    }

    // Добавляем сервис
    esp_err_t err = mdns_service_add(instance_name,
                                     service_type,
                                     proto,
                                     port,
                                     mdns_txt,
                                     txt_count);

    if (mdns_txt)
        free(mdns_txt);
    if (hostname)
        free(hostname);

    if (err == ESP_OK)
    {
        ESP_LOGI(MDNS_TAG, "Service added: %s.%s.%s on port %d",
                 instance_name, service_type, proto, port);
    }
    else
    {
        ESP_LOGE(MDNS_TAG, "Failed to add service: %d", err);
    }

    return err;
}

// Специальная функция для Home Assistant discovery
esp_err_t um_mdns_add_discovery(void)
{
    um_mdns_txt_item_t discovery[] = {
        {"unique_id", NULL},
        {"name", NULL},
        {"capabilities", NULL}};

    // Получаем hostname для unique_id
    char *hostname = NULL;
    um_nvs_get_hostname(&hostname);

    char unique_id[64];
    char name[64];

    if (hostname)
    {
        snprintf(unique_id, sizeof(unique_id), "%s", hostname);
        snprintf(name, sizeof(name), "UMNI %s", hostname);
        discovery[0].value = unique_id; // unique_id
        discovery[1].value = name;      // name
    }
    else
    {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(unique_id, sizeof(unique_id), "umni_%02x%02x%02x", mac[3], mac[4], mac[5]);
        discovery[0].value = unique_id;
        discovery[1].value = "UMNI Device";
    }
    esp_err_t err = um_mdns_add_service("UMNI UNI REST API",
                                        "_umni_api",
                                        "_tcp",
                                        80,
                                        discovery,
                                        2);

    if (hostname)
        free(hostname);

    return err;
}
