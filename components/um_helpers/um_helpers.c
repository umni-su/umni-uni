#include <string.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "lwip/ip_addr.h"
#include "um_helpers.h"

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
    esp_netif_t *netif = esp_netif_next(NULL);

    while (netif != NULL && count < max_count)
    {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        {
            um_network_interface_info_t *info = &interfaces[count];
            const char *ifkey = esp_netif_get_ifkey(netif);

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

            // Заполняем сетевые параметры
            snprintf(info->ip_address, sizeof(info->ip_address), IPSTR, IP2STR(&ip_info.ip));
            snprintf(info->netmask, sizeof(info->netmask), IPSTR, IP2STR(&ip_info.netmask));
            snprintf(info->gateway, sizeof(info->gateway), IPSTR, IP2STR(&ip_info.gw));

            // Интерфейс считаем активным, если поднят линк и получен IP
            // Дополнительно можно проверить esp_netif_is_netif_up(netif)
            info->is_active = (ip_info.ip.addr != 0) ? 1 : 0;

            count++;
        }
        netif = esp_netif_next(netif);
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
