#include <string.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "lwip/ip_addr.h"
#include "um_helpers.h"
#include "um_nvs.h"
#include "um_capabilities.h"

#define DEVICE_NAME_PREFIX "umni-"
#define DEVICE_NAME_MAX_LEN 32

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

esp_err_t um_helperts_get_systeminfo(cJSON **data)
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