#include <string.h>
#include "esp_mac.h"
#include "esp_log.h"

#define DEVICE_NAME_PREFIX "umni-"
#define DEVICE_NAME_MAX_LEN 32

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
