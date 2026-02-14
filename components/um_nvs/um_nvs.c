/**
 * @file um_nvs.c
 * @brief Non-volatile storage management implementation
 * @version 2.0.0
 */

#include <stdlib.h>
#include <string.h>
#include "um_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "nvs";

static nvs_handle_t um_nvs_handle = 0;
static char *current_namespace = NULL;

/* Forward declarations */
static esp_err_t commit_changes(void);

/* String size limit for safety */
#define NVS_MAX_STR_SIZE 1024

/**
 * @brief Initialize NVS flash storage
 */
esp_err_t um_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = um_nvs_open("um_nvs");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open default namespace");
        return err;
    }

    ESP_LOGI(TAG, "NVS initialized successfully");
    return ESP_OK;
}

/**
 * @brief Open NVS namespace
 */
esp_err_t um_nvs_open(const char *namespace)
{
    if (namespace == NULL)
    {
        ESP_LOGE(TAG, "Namespace cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Закрыть предыдущий namespace если открыт
    if (um_nvs_handle != 0)
    {
        ESP_LOGD(TAG, "Closing previous namespace: %s",
                 current_namespace ? current_namespace : "NULL");
        nvs_close(um_nvs_handle);
        um_nvs_handle = 0;
    }

    // Освободить предыдущее имя
    if (current_namespace != NULL)
    {
        free(current_namespace);
        current_namespace = NULL;
    }

    esp_err_t err = nvs_open(namespace, NVS_READWRITE, &um_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s",
                 namespace, esp_err_to_name(err));
        return err;
    }

    current_namespace = strdup(namespace);
    if (current_namespace == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for namespace");
        nvs_close(um_nvs_handle);
        um_nvs_handle = 0;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Opened NVS namespace: %s", namespace);
    return ESP_OK;
}

/**
 * @brief Close NVS namespace
 */
void um_nvs_close(void)
{
    if (um_nvs_handle != 0)
    {
        nvs_close(um_nvs_handle);
        um_nvs_handle = 0;
    }

    if (current_namespace != NULL)
    {
        free(current_namespace);
        current_namespace = NULL;
    }

    ESP_LOGI(TAG, "NVS namespace closed");
}

/**
 * @brief Check if namespace is currently open
 */
bool um_nvs_is_open(void)
{
    return (um_nvs_handle != 0);
}

/**
 * @brief Check if system is installed
 */
bool um_nvs_is_installed(void)
{
    int8_t installed = 0;
    char *username = NULL;
    char *password = NULL;

    esp_err_t err1 = um_nvs_read_i8(UM_NVS_KEY_INSTALLED, &installed);
    esp_err_t err2 = um_nvs_read_str(UM_NVS_KEY_USERNAME, &username);
    esp_err_t err3 = um_nvs_read_str(UM_NVS_KEY_PASSWORD, &password);

    bool result = (err1 == ESP_OK && installed == 1) &&
                  (err2 == ESP_OK && username != NULL) &&
                  (err3 == ESP_OK && password != NULL);

    free(username);
    free(password);

    return result;
}

/**
 * @brief Erase all data in current namespace
 */
esp_err_t um_nvs_erase(void)
{
    if (um_nvs_handle == 0)
    {
        ESP_LOGE(TAG, "NVS not opened");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = nvs_erase_all(um_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = commit_changes();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit erase: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "NVS erased successfully");
    return ESP_OK;
}

/**
 * @brief Delete specific key from NVS
 */
esp_err_t um_nvs_delete_key(const char *key)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_erase_key(um_nvs_handle, key);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete key '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    return commit_changes();
}

/**
 * @brief Commit changes to NVS
 */
static esp_err_t commit_changes(void)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = nvs_commit(um_nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to commit changes: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * @brief Initialize NVS with default values
 */
esp_err_t um_nvs_initialize_with_defaults(void)
{
    esp_err_t err = ESP_OK;
    esp_err_t res = ESP_OK;

    res = um_nvs_write_i8(UM_NVS_KEY_ETH_TYPE, UM_NVS_IP_TYPE_DHCP);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_i8(UM_NVS_KEY_WIFI_TYPE, UM_NVS_IP_TYPE_DHCP);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_i8(UM_NVS_KEY_UPDATES_CHANNEL, 1);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_str(UM_NVS_KEY_NTP, UM_NVS_DEFAULT_NTP);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_str(UM_NVS_KEY_TIMEZONE, UM_NVS_DEFAULT_TIMEZONE);
    if (res != ESP_OK)
        err = res;

    /* OpenTherm defaults */
    res = um_nvs_write_i8(UM_NVS_KEY_OT_CH, UM_NVS_DEFAULT_OT_CH);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_i8(UM_NVS_KEY_OT_CH_SETPOINT, UM_NVS_DEFAULT_OT_CH_SETPOINT);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_i8(UM_NVS_KEY_OT_DHW_SETPOINT, UM_NVS_DEFAULT_OT_DHW_SETPOINT);
    if (res != ESP_OK)
        err = res;

    res = um_nvs_write_i8(UM_NVS_KEY_OT_DHW, UM_NVS_DEFAULT_OT_DHW);
    if (res != ESP_OK)
        err = res;

    /* Network defaults */
    res = um_nvs_write_i8(UM_NVS_KEY_NETWORK_MODE, UM_NVS_DEFAULT_NETWORK_MODE);
    if (res != ESP_OK)
        err = res;

    /* MQTT defaults */
    res = um_nvs_write_u16(UM_NVS_KEY_MQTT_PORT, UM_NVS_DEFAULT_MQTT_PORT);
    if (res != ESP_OK)
        err = res;

    ESP_LOGI(TAG, "NVS initialized with default values");
    return err;
}

/* Generic read functions */

esp_err_t um_nvs_read_i8(const char *key, int8_t *out_value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_get_i8(um_nvs_handle, key, out_value);
    if (err != ESP_OK)
    {
        ESP_LOGD(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Read i8: %s = %d", key, *out_value);
    return ESP_OK;
}

esp_err_t um_nvs_read_i16(const char *key, int16_t *out_value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_get_i16(um_nvs_handle, key, out_value);
    if (err != ESP_OK)
    {
        ESP_LOGD(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Read i16: %s = %d", key, *out_value);
    return ESP_OK;
}

esp_err_t um_nvs_read_i64(const char *key, int64_t *out_value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_get_i64(um_nvs_handle, key, out_value);
    if (err != ESP_OK)
    {
        ESP_LOGD(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Read i64: %s = %lld", key, *out_value);
    return ESP_OK;
}

esp_err_t um_nvs_read_u16(const char *key, uint16_t *out_value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_get_u16(um_nvs_handle, key, out_value);
    if (err != ESP_OK)
    {
        ESP_LOGD(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Read u16: %s = %u", key, *out_value);
    return ESP_OK;
}

esp_err_t um_nvs_read_str(const char *key, char **out_value)
{
    return um_nvs_read_str_len(key, out_value, NVS_MAX_STR_SIZE);
}

esp_err_t um_nvs_read_str_len(const char *key, char **out_value, size_t max_len)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL || out_value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Инициализируем выходной параметр
    *out_value = NULL;

    // Получаем размер строки
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(um_nvs_handle, key, NULL, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGD(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return err;
    }

    // Проверяем размер
    if (required_size == 0)
    {
        // Пустая строка - это валидное значение
        *out_value = strdup("");
        if (*out_value == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    if (required_size > max_len)
    {
        ESP_LOGE(TAG, "String too long for key '%s': %d bytes (max %d)",
                 key, required_size, max_len);
        return ESP_ERR_INVALID_SIZE;
    }

    // Выделяем память
    char *value = malloc(required_size);
    if (value == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for string");
        return ESP_ERR_NO_MEM;
    }

    // Читаем строку
    err = nvs_get_str(um_nvs_handle, key, value, &required_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read string '%s': %s", key, esp_err_to_name(err));
        free(value);
        return err;
    }

    *out_value = value;
    ESP_LOGD(TAG, "Read str: %s = %s", key, value);
    return ESP_OK;
}

/* Generic write functions */

esp_err_t um_nvs_write_i8(const char *key, int8_t value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_set_i8(um_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write i8 '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = commit_changes();
    ESP_LOGD(TAG, "Write i8: %s = %d", key, value);
    return err;
}

esp_err_t um_nvs_write_i16(const char *key, int16_t value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_set_i16(um_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write i16 '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = commit_changes();
    ESP_LOGD(TAG, "Write i16: %s = %d", key, value);
    return err;
}

esp_err_t um_nvs_write_u16(const char *key, uint16_t value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_set_u16(um_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write u16 '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = commit_changes();
    ESP_LOGD(TAG, "Write u16: %s = %u", key, value);
    return err;
}

esp_err_t um_nvs_write_i64(const char *key, int64_t value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = nvs_set_i64(um_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write i64 '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = commit_changes();
    ESP_LOGD(TAG, "Write i64: %s = %lld", key, value);
    return err;
}

esp_err_t um_nvs_write_str(const char *key, const char *value)
{
    if (um_nvs_handle == 0)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (key == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (value == NULL)
    {
        // Удаляем ключ если значение NULL
        return um_nvs_delete_key(key);
    }

    esp_err_t err = nvs_set_str(um_nvs_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write str '%s': %s", key, esp_err_to_name(err));
        return err;
    }

    err = commit_changes();
    ESP_LOGD(TAG, "Write str: %s = %s", key, value);
    return err;
}

/* Legacy getters implementation (backward compatibility) */

bool um_nvs_get_installed(void)
{
    return um_nvs_is_installed();
}

/* System Getters */
esp_err_t um_nvs_get_hostname(char **hostname)
{
    return um_nvs_read_str(UM_NVS_KEY_HOSTNAME, hostname);
}

esp_err_t um_nvs_get_macname(char **macname)
{
    return um_nvs_read_str(UM_NVS_KEY_MACNAME, macname);
}

esp_err_t um_nvs_get_username(char **username)
{
    return um_nvs_read_str(UM_NVS_KEY_USERNAME, username);
}

esp_err_t um_nvs_get_password(char **password)
{
    return um_nvs_read_str(UM_NVS_KEY_PASSWORD, password);
}

esp_err_t um_nvs_get_ntp(char **ntp)
{
    return um_nvs_read_str(UM_NVS_KEY_NTP, ntp);
}

esp_err_t um_nvs_get_updates_channel(uint8_t *channel)
{
    if (channel == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_UPDATES_CHANNEL, &value);
    if (err == ESP_OK)
    {
        *channel = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_timezone(char **timezone)
{
    return um_nvs_read_str(UM_NVS_KEY_TIMEZONE, timezone);
}

esp_err_t um_nvs_get_poweron_at(char **poweron_at)
{
    return um_nvs_read_str(UM_NVS_KEY_POWERON_AT, poweron_at);
}

esp_err_t um_nvs_get_reset_at(char **reset_at)
{
    return um_nvs_read_str(UM_NVS_KEY_RESET_AT, reset_at);
}

/* System Setters */
esp_err_t um_nvs_set_installed(bool installed)
{
    return um_nvs_write_i8(UM_NVS_KEY_INSTALLED, installed ? 1 : 0);
}

esp_err_t um_nvs_set_hostname(const char *hostname)
{
    return um_nvs_write_str(UM_NVS_KEY_HOSTNAME, hostname);
}

esp_err_t um_nvs_set_macname(const char *macname)
{
    return um_nvs_write_str(UM_NVS_KEY_MACNAME, macname);
}

esp_err_t um_nvs_set_username(const char *username)
{
    return um_nvs_write_str(UM_NVS_KEY_USERNAME, username);
}

esp_err_t um_nvs_set_password(const char *password)
{
    return um_nvs_write_str(UM_NVS_KEY_PASSWORD, password);
}

esp_err_t um_nvs_set_ntp(const char *ntp)
{
    return um_nvs_write_str(UM_NVS_KEY_NTP, ntp);
}

esp_err_t um_nvs_set_updates_channel(uint8_t channel)
{
    return um_nvs_write_i8(UM_NVS_KEY_UPDATES_CHANNEL, (int8_t)channel);
}

esp_err_t um_nvs_set_timezone(const char *timezone)
{
    return um_nvs_write_str(UM_NVS_KEY_TIMEZONE, timezone);
}

esp_err_t um_nvs_set_poweron_at(const char *poweron_at)
{
    return um_nvs_write_str(UM_NVS_KEY_POWERON_AT, poweron_at);
}

esp_err_t um_nvs_set_reset_at(const char *reset_at)
{
    return um_nvs_write_str(UM_NVS_KEY_RESET_AT, reset_at);
}

esp_err_t um_nvs_get_webserver_token(char **token)
{
    return um_nvs_read_str(UM_NVS_KEY_WEBSERVER_TOKEN, token);
}

esp_err_t um_nvs_set_webserver_token(char *token)
{
    return um_nvs_write_str(UM_NVS_KEY_WEBSERVER_TOKEN, token);
}

/* Network Getters */
esp_err_t um_nvs_get_network_mode(uint8_t *mode)
{
    if (mode == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_NETWORK_MODE, &value);
    if (err == ESP_OK)
    {
        *mode = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_wifi_sta_mac(char **mac)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_STA_MAC, mac);
}

esp_err_t um_nvs_get_wifi_sta_ssid(char **ssid)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_STA_SSID, ssid);
}

esp_err_t um_nvs_get_wifi_sta_password(char **password)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_STA_PWD, password);
}

esp_err_t um_nvs_get_wifi_mac(char **mac)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_MAC, mac);
}

esp_err_t um_nvs_get_wifi_type(uint8_t *type)
{
    if (type == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_WIFI_TYPE, &value);
    if (err == ESP_OK)
    {
        *type = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_wifi_ip(char **ip)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_IP, ip);
}

esp_err_t um_nvs_get_wifi_netmask(char **netmask)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_NETMASK, netmask);
}

esp_err_t um_nvs_get_wifi_gateway(char **gateway)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_GATEWAY, gateway);
}

esp_err_t um_nvs_get_wifi_dns(char **dns)
{
    return um_nvs_read_str(UM_NVS_KEY_WIFI_DNS, dns);
}

esp_err_t um_nvs_get_eth_mac(char **mac)
{
    return um_nvs_read_str(UM_NVS_KEY_ETH_MAC, mac);
}

esp_err_t um_nvs_get_eth_type(uint8_t *type)
{
    if (type == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_ETH_TYPE, &value);
    if (err == ESP_OK)
    {
        *type = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_eth_ip(char **ip)
{
    return um_nvs_read_str(UM_NVS_KEY_ETH_IP, ip);
}

esp_err_t um_nvs_get_eth_netmask(char **netmask)
{
    return um_nvs_read_str(UM_NVS_KEY_ETH_NETMASK, netmask);
}

esp_err_t um_nvs_get_eth_gateway(char **gateway)
{
    return um_nvs_read_str(UM_NVS_KEY_ETH_GATEWAY, gateway);
}

esp_err_t um_nvs_get_eth_dns(char **dns)
{
    return um_nvs_read_str(UM_NVS_KEY_ETH_DNS, dns);
}

/* Network Setters */
esp_err_t um_nvs_set_network_mode(uint8_t mode)
{
    return um_nvs_write_i8(UM_NVS_KEY_NETWORK_MODE, (int8_t)mode);
}

esp_err_t um_nvs_set_wifi_sta_mac(const char *mac)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_STA_MAC, mac);
}

esp_err_t um_nvs_set_wifi_sta_ssid(const char *ssid)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_STA_SSID, ssid);
}

esp_err_t um_nvs_set_wifi_sta_password(const char *password)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_STA_PWD, password);
}

esp_err_t um_nvs_set_wifi_mac(const char *mac)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_MAC, mac);
}

esp_err_t um_nvs_set_wifi_type(uint8_t type)
{
    return um_nvs_write_i8(UM_NVS_KEY_WIFI_TYPE, (int8_t)type);
}

esp_err_t um_nvs_set_wifi_ip(const char *ip)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_IP, ip);
}

esp_err_t um_nvs_set_wifi_netmask(const char *netmask)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_NETMASK, netmask);
}

esp_err_t um_nvs_set_wifi_gateway(const char *gateway)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_GATEWAY, gateway);
}

esp_err_t um_nvs_set_wifi_dns(const char *dns)
{
    return um_nvs_write_str(UM_NVS_KEY_WIFI_DNS, dns);
}

esp_err_t um_nvs_set_eth_mac(const char *mac)
{
    return um_nvs_write_str(UM_NVS_KEY_ETH_MAC, mac);
}

esp_err_t um_nvs_set_eth_type(uint8_t type)
{
    return um_nvs_write_i8(UM_NVS_KEY_ETH_TYPE, (int8_t)type);
}

esp_err_t um_nvs_set_eth_ip(const char *ip)
{
    return um_nvs_write_str(UM_NVS_KEY_ETH_IP, ip);
}

esp_err_t um_nvs_set_eth_netmask(const char *netmask)
{
    return um_nvs_write_str(UM_NVS_KEY_ETH_NETMASK, netmask);
}

esp_err_t um_nvs_set_eth_gateway(const char *gateway)
{
    return um_nvs_write_str(UM_NVS_KEY_ETH_GATEWAY, gateway);
}

esp_err_t um_nvs_set_eth_dns(const char *dns)
{
    return um_nvs_write_str(UM_NVS_KEY_ETH_DNS, dns);
}

/* OpenTherm Getters */
esp_err_t um_nvs_get_ot_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_EN, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_ot_ch_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_CH, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_ot_ch2_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_CH2, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_ot_ch_setpoint(uint8_t *setpoint)
{
    if (setpoint == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_CH_SETPOINT, &value);
    if (err == ESP_OK)
    {
        *setpoint = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_ot_dhw_setpoint(uint8_t *setpoint)
{
    if (setpoint == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_DHW_SETPOINT, &value);
    if (err == ESP_OK)
    {
        *setpoint = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_ot_dhw_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_DHW, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_ot_cool_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_COOL, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_ot_modulation(uint8_t *modulation)
{
    if (modulation == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_MOD, &value);
    if (err == ESP_OK)
    {
        *modulation = (uint8_t)value;
    }
    return err;
}

esp_err_t um_nvs_get_ot_outdoor_temp_comp(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_OTC, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_ot_heating_curve_ratio(uint8_t *ratio)
{
    if (ratio == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OT_HCR, &value);
    if (err == ESP_OK)
    {
        *ratio = (uint8_t)value;
    }
    return err;
}

/* OpenTherm Setters */
esp_err_t um_nvs_set_ot_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_EN, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_ot_ch_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_CH, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_ot_ch2_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_CH2, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_ot_ch_setpoint(uint8_t setpoint)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_CH_SETPOINT, (int8_t)setpoint);
}

esp_err_t um_nvs_set_ot_dhw_setpoint(uint8_t setpoint)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_DHW_SETPOINT, (int8_t)setpoint);
}

esp_err_t um_nvs_set_ot_dhw_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_DHW, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_ot_cool_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_COOL, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_ot_modulation(uint8_t modulation)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_MOD, (int8_t)modulation);
}

esp_err_t um_nvs_set_ot_outdoor_temp_comp(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_OTC, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_ot_heating_curve_ratio(uint8_t ratio)
{
    return um_nvs_write_i8(UM_NVS_KEY_OT_HCR, (int8_t)ratio);
}

/* Outputs Getters */
esp_err_t um_nvs_get_outputs_data(uint8_t *data)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_OUTPUTS_DATA, &value);
    if (err == ESP_OK)
    {
        *data = (uint8_t)value;
    }
    return err;
}

/* Outputs Setters */
esp_err_t um_nvs_set_outputs_data(uint8_t data)
{
    return um_nvs_write_i8(UM_NVS_KEY_OUTPUTS_DATA, (int8_t)data);
}

/* MQTT Getters */
esp_err_t um_nvs_get_mqtt_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_MQTT_ENABLED, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_mqtt_host(char **host)
{
    return um_nvs_read_str(UM_NVS_KEY_MQTT_HOST, host);
}

esp_err_t um_nvs_get_mqtt_port(uint16_t *port)
{
    if (port == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return um_nvs_read_u16(UM_NVS_KEY_MQTT_PORT, port);
}

esp_err_t um_nvs_get_mqtt_username(char **username)
{
    return um_nvs_read_str(UM_NVS_KEY_MQTT_USER, username);
}

esp_err_t um_nvs_get_mqtt_password(char **password)
{
    return um_nvs_read_str(UM_NVS_KEY_MQTT_PWD, password);
}

/* MQTT Setters */
esp_err_t um_nvs_set_mqtt_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_MQTT_ENABLED, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_mqtt_host(const char *host)
{
    return um_nvs_write_str(UM_NVS_KEY_MQTT_HOST, host);
}

esp_err_t um_nvs_set_mqtt_port(uint16_t port)
{
    return um_nvs_write_u16(UM_NVS_KEY_MQTT_PORT, port);
}

esp_err_t um_nvs_set_mqtt_username(const char *username)
{
    return um_nvs_write_str(UM_NVS_KEY_MQTT_USER, username);
}

esp_err_t um_nvs_set_mqtt_password(const char *password)
{
    return um_nvs_write_str(UM_NVS_KEY_MQTT_PWD, password);
}

/* Webhooks Getters */
esp_err_t um_nvs_get_webhooks_enabled(bool *enabled)
{
    if (enabled == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int8_t value = 0;
    esp_err_t err = um_nvs_read_i8(UM_NVS_KEY_WEBHOOKS, &value);
    if (err == ESP_OK)
    {
        *enabled = (value == 1);
    }
    return err;
}

esp_err_t um_nvs_get_webhooks_url(char **url)
{
    return um_nvs_read_str(UM_NVS_KEY_WEBHOOKS_URL, url);
}

/* Webhooks Setters */
esp_err_t um_nvs_set_webhooks_enabled(bool enabled)
{
    return um_nvs_write_i8(UM_NVS_KEY_WEBHOOKS, enabled ? 1 : 0);
}

esp_err_t um_nvs_set_webhooks_url(const char *url)
{
    return um_nvs_write_str(UM_NVS_KEY_WEBHOOKS_URL, url);
}