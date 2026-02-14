/**
 * @file um_nvs.h
 * @brief Non-volatile storage management component
 * @version 1.0.0
 */

#ifndef UM_NVS_H
#define UM_NVS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* NVS Key Definitions */
#define UM_NVS_KEY_INSTALLED "inst"
#define UM_NVS_KEY_HOSTNAME "name"
#define UM_NVS_KEY_MACNAME "macname"
#define UM_NVS_KEY_USERNAME "admusr"
#define UM_NVS_KEY_PASSWORD "admpwd"
#define UM_NVS_KEY_NTP "ntp"
#define UM_NVS_KEY_UPDATES_CHANNEL "upd"
#define UM_NVS_KEY_TIMEZONE "tz"
#define UM_NVS_KEY_POWERON_AT "poweron"
#define UM_NVS_KEY_RESET_AT "resetat"
#define UM_NVS_KEY_NETWORK_MODE "nwmode"
#define UM_NVS_KEY_WIFI_STA_MAC "stmac"
#define UM_NVS_KEY_WIFI_STA_SSID "stname"
#define UM_NVS_KEY_WIFI_STA_PWD "stpwd"
#define UM_NVS_KEY_WIFI_MAC "wmac"
#define UM_NVS_KEY_WIFI_TYPE "wt"
#define UM_NVS_KEY_WIFI_IP "wip"
#define UM_NVS_KEY_WIFI_NETMASK "wnm"
#define UM_NVS_KEY_WIFI_GATEWAY "wgw"
#define UM_NVS_KEY_WIFI_DNS "wdns"
#define UM_NVS_KEY_ETH_MAC "emac"
#define UM_NVS_KEY_ETH_TYPE "et"
#define UM_NVS_KEY_ETH_IP "eip"
#define UM_NVS_KEY_ETH_NETMASK "enm"
#define UM_NVS_KEY_ETH_GATEWAY "egw"
#define UM_NVS_KEY_ETH_DNS "edns"
#define UM_NVS_KEY_OT_EN "oten"
#define UM_NVS_KEY_OT_CH "otch"
#define UM_NVS_KEY_OT_CH2 "otch2"
#define UM_NVS_KEY_OT_CH_SETPOINT "ottbsp"
#define UM_NVS_KEY_OT_DHW_SETPOINT "otdhwsp"
#define UM_NVS_KEY_OT_DHW "otdhw"
#define UM_NVS_KEY_OT_COOL "otcol"
#define UM_NVS_KEY_OT_MOD "otmod"
#define UM_NVS_KEY_OT_OTC "ototc"
#define UM_NVS_KEY_OT_HCR "othcr"
#define UM_NVS_KEY_OUTPUTS_DATA "outputs"
#define UM_NVS_KEY_MQTT_ENABLED "mqen"
#define UM_NVS_KEY_MQTT_HOST "mqhost"
#define UM_NVS_KEY_MQTT_PORT "mqport"
#define UM_NVS_KEY_MQTT_USER "mquser"
#define UM_NVS_KEY_MQTT_PWD "mqpwd"
#define UM_NVS_KEY_WEBHOOKS "whk"
#define UM_NVS_KEY_WEBHOOKS_URL "whkurl"
#define UM_NVS_KEY_OPENCOLLECTORS "ocols"
#define UM_NVS_KEY_WEBSERVER_TOKEN "httptoken"

/* Default Values */
#define UM_NVS_DEFAULT_NTP "0.ru.pool.ntp.org"
#define UM_NVS_DEFAULT_TIMEZONE "MSK-3"
#define UM_NVS_DEFAULT_NETWORK_MODE 1
#define UM_NVS_DEFAULT_WIFI_TYPE 1
#define UM_NVS_DEFAULT_ETH_TYPE 1
#define UM_NVS_DEFAULT_OT_EN 0
#define UM_NVS_DEFAULT_OT_CH 1
#define UM_NVS_DEFAULT_OT_CH2 0
#define UM_NVS_DEFAULT_OT_CH_SETPOINT 45
#define UM_NVS_DEFAULT_OT_DHW_SETPOINT 60
#define UM_NVS_DEFAULT_OT_DHW 1
#define UM_NVS_DEFAULT_OT_COOL 0
#define UM_NVS_DEFAULT_OT_MOD 99
#define UM_NVS_DEFAULT_OT_OTC 0
#define UM_NVS_DEFAULT_OUTPUTS_DATA 0
#define UM_NVS_DEFAULT_MQTT_ENABLED 0
#define UM_NVS_DEFAULT_MQTT_PORT 1883
#define UM_NVS_DEFAULT_WEBHOOKS 0

/* Network Mode Definitions */
#define UM_NVS_NETWORK_MODE_ETH 1
#define UM_NVS_NETWORK_MODE_WIFI_AP 2
#define UM_NVS_NETWORK_MODE_WIFI_STA 3

/* IP Type Definitions */
#define UM_NVS_IP_TYPE_DHCP 1
#define UM_NVS_IP_TYPE_STATIC 2

// Маска для битового хранения
#define OC1_STATE_MASK 0x01 // bit 0
#define OC2_STATE_MASK 0x02 // bit 1

    /**
     * @brief Initialize NVS flash storage
     *
     * This function initializes the NVS flash storage. It must be called
     * before any other NVS operations.
     *
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_init(void);

    /**
     * @brief Open a namespace in NVS
     *
     * Opens a specific namespace for read/write operations. Only one namespace
     * can be open at a time.
     *
     * @param namespace Name of the namespace to open
     * @return ESP_OK on success, error code on failure
     * @note If a namespace is already open, it will be closed first
     */
    esp_err_t um_nvs_open(const char *namespace);

    /**
     * @brief Close the current namespace
     *
     * Closes the currently open namespace and frees associated resources.
     */
    void um_nvs_close(void);

    /**
     * @brief Check if namespace is currently open
     *
     * @return true if a namespace is open, false otherwise
     */
    bool um_nvs_is_open(void);

    /**
     * @brief Check if system is installed
     *
     * System is considered installed if all required credentials are set.
     *
     * @return true if system is installed, false otherwise
     */
    bool um_nvs_is_installed(void);

    /**
     * @brief Erase all data in current namespace
     *
     * @warning This operation is irreversible!
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_erase(void);

    /**
     * @brief Delete a specific key from NVS
     *
     * @param key Key to delete
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_delete_key(const char *key);

    /**
     * @brief Initialize NVS with default values
     *
     * Sets all configuration parameters to their default values.
     *
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_initialize_with_defaults(void);

    /* Generic read functions */

    /**
     * @brief Read 8-bit signed integer from NVS
     *
     * @param key Key to read
     * @param[out] out_value Pointer to store the value
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_read_i8(const char *key, int8_t *out_value);

    /**
     * @brief Read 16-bit signed integer from NVS
     *
     * @param key Key to read
     * @param[out] out_value Pointer to store the value
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_read_i16(const char *key, int16_t *out_value);

    /**
     * @brief Read 64-bit signed integer from NVS
     *
     * @param key Key to read
     * @param[out] out_value Pointer to store the value
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_read_i64(const char *key, int64_t *out_value);

    /**
     * @brief Read 16-bit unsigned integer from NVS
     *
     * @param key Key to read
     * @param[out] out Pointer to store the value
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_read_u16(const char *key, uint16_t *out_value);

    /**
     * @brief Read string from NVS
     *
     * @param key Key to read
     * @param[out] out_value Pointer to store the allocated string
     * @return ESP_OK on success, error code on failure
     * @note Caller must free the returned string using free()
     * @note *out_value will be set to NULL on error
     */
    esp_err_t um_nvs_read_str(const char *key, char **out_value);

    /**
     * @brief Read string from NVS with maximum length limit
     *
     * @param key Key to read
     * @param[out] out_value Pointer to store the allocated string
     * @param max_len Maximum allowed string length (including null terminator)
     * @return ESP_OK on success, error code on failure
     * @note Caller must free the returned string using free()
     * @note *out_value will be set to NULL on error
     */
    esp_err_t um_nvs_read_str_len(const char *key, char **out_value, size_t max_len);

    /* Generic write functions */

    /**
     * @brief Write 8-bit signed integer to NVS
     *
     * @param key Key to write
     * @param value Value to write
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_write_i8(const char *key, int8_t value);

    /**
     * @brief Write 16-bit signed integer to NVS
     *
     * @param key Key to write
     * @param value Value to write
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_write_i16(const char *key, int16_t value);

    /**
     * @brief Write 16-bit unsigned integer to NVS
     *
     * @param key Key to write
     * @param value Value to write
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_write_u16(const char *key, uint16_t value);

    /**
     * @brief Write 64-bit signed integer to NVS
     *
     * @param key Key to write
     * @param value Value to write
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_write_i64(const char *key, int64_t value);

    /**
     * @brief Write string to NVS
     *
     * @param key Key to write
     * @param value String to write (NULL to delete the key)
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t um_nvs_write_str(const char *key, const char *value);

    /* Legacy getters (for backward compatibility - return default values on error) */

    /* System Getters */
    bool um_nvs_get_installed(void);
    esp_err_t um_nvs_get_hostname(char **hostname);
    esp_err_t um_nvs_get_macname(char **macname);
    esp_err_t um_nvs_get_username(char **username);
    esp_err_t um_nvs_get_password(char **password);
    esp_err_t um_nvs_get_ntp(char **ntp);
    esp_err_t um_nvs_get_updates_channel(uint8_t *channel);
    esp_err_t um_nvs_get_timezone(char **timezone);
    esp_err_t um_nvs_get_poweron_at(char **poweron_at);
    esp_err_t um_nvs_get_reset_at(char **reset_at);

    /* System Setters */
    esp_err_t um_nvs_set_installed(bool installed);
    esp_err_t um_nvs_set_hostname(const char *hostname);
    esp_err_t um_nvs_set_macname(const char *macname);
    esp_err_t um_nvs_set_username(const char *username);
    esp_err_t um_nvs_set_password(const char *password);
    esp_err_t um_nvs_set_ntp(const char *ntp);
    esp_err_t um_nvs_set_updates_channel(uint8_t channel);
    esp_err_t um_nvs_set_timezone(const char *timezone);
    esp_err_t um_nvs_set_poweron_at(const char *poweron_at);
    esp_err_t um_nvs_set_reset_at(const char *reset_at);

    /* Webserver */
    esp_err_t um_nvs_get_webserver_token(char **token);
    esp_err_t um_nvs_set_webserver_token(char *token);

    /* Network Getters */
    esp_err_t um_nvs_get_network_mode(uint8_t *mode);
    esp_err_t um_nvs_get_wifi_sta_mac(char **mac);
    esp_err_t um_nvs_get_wifi_sta_ssid(char **ssid);
    esp_err_t um_nvs_get_wifi_sta_password(char **password);
    esp_err_t um_nvs_get_wifi_mac(char **mac);
    esp_err_t um_nvs_get_wifi_type(uint8_t *type);
    esp_err_t um_nvs_get_wifi_ip(char **ip);
    esp_err_t um_nvs_get_wifi_netmask(char **netmask);
    esp_err_t um_nvs_get_wifi_gateway(char **gateway);
    esp_err_t um_nvs_get_wifi_dns(char **dns);
    esp_err_t um_nvs_get_eth_mac(char **mac);
    esp_err_t um_nvs_get_eth_type(uint8_t *type);
    esp_err_t um_nvs_get_eth_ip(char **ip);
    esp_err_t um_nvs_get_eth_netmask(char **netmask);
    esp_err_t um_nvs_get_eth_gateway(char **gateway);
    esp_err_t um_nvs_get_eth_dns(char **dns);

    /* Network Setters */
    esp_err_t um_nvs_set_network_mode(uint8_t mode);
    esp_err_t um_nvs_set_wifi_sta_mac(const char *mac);
    esp_err_t um_nvs_set_wifi_sta_ssid(const char *ssid);
    esp_err_t um_nvs_set_wifi_sta_password(const char *password);
    esp_err_t um_nvs_set_wifi_mac(const char *mac);
    esp_err_t um_nvs_set_wifi_type(uint8_t type);
    esp_err_t um_nvs_set_wifi_ip(const char *ip);
    esp_err_t um_nvs_set_wifi_netmask(const char *netmask);
    esp_err_t um_nvs_set_wifi_gateway(const char *gateway);
    esp_err_t um_nvs_set_wifi_dns(const char *dns);
    esp_err_t um_nvs_set_eth_mac(const char *mac);
    esp_err_t um_nvs_set_eth_type(uint8_t type);
    esp_err_t um_nvs_set_eth_ip(const char *ip);
    esp_err_t um_nvs_set_eth_netmask(const char *netmask);
    esp_err_t um_nvs_set_eth_gateway(const char *gateway);
    esp_err_t um_nvs_set_eth_dns(const char *dns);

    /* OpenTherm Getters */
    esp_err_t um_nvs_get_ot_enabled(bool *enabled);
    esp_err_t um_nvs_get_ot_ch_enabled(bool *enabled);
    esp_err_t um_nvs_get_ot_ch2_enabled(bool *enabled);
    esp_err_t um_nvs_get_ot_ch_setpoint(uint8_t *setpoint);
    esp_err_t um_nvs_get_ot_dhw_setpoint(uint8_t *setpoint);
    esp_err_t um_nvs_get_ot_dhw_enabled(bool *enabled);
    esp_err_t um_nvs_get_ot_cool_enabled(bool *enabled);
    esp_err_t um_nvs_get_ot_modulation(uint8_t *modulation);
    esp_err_t um_nvs_get_ot_outdoor_temp_comp(bool *enabled);
    esp_err_t um_nvs_get_ot_heating_curve_ratio(uint8_t *ratio);

    /* OpenTherm Setters */
    esp_err_t um_nvs_set_ot_enabled(bool enabled);
    esp_err_t um_nvs_set_ot_ch_enabled(bool enabled);
    esp_err_t um_nvs_set_ot_ch2_enabled(bool enabled);
    esp_err_t um_nvs_set_ot_ch_setpoint(uint8_t setpoint);
    esp_err_t um_nvs_set_ot_dhw_setpoint(uint8_t setpoint);
    esp_err_t um_nvs_set_ot_dhw_enabled(bool enabled);
    esp_err_t um_nvs_set_ot_cool_enabled(bool enabled);
    esp_err_t um_nvs_set_ot_modulation(uint8_t modulation);
    esp_err_t um_nvs_set_ot_outdoor_temp_comp(bool enabled);
    esp_err_t um_nvs_set_ot_heating_curve_ratio(uint8_t ratio);

    /* Outputs Getters */
    esp_err_t um_nvs_get_outputs_data(uint8_t *data);

    /* Outputs Setters */
    esp_err_t um_nvs_set_outputs_data(uint8_t data);

    /* MQTT Getters */
    esp_err_t um_nvs_get_mqtt_enabled(bool *enabled);
    esp_err_t um_nvs_get_mqtt_host(char **host);
    esp_err_t um_nvs_get_mqtt_port(uint16_t *port);
    esp_err_t um_nvs_get_mqtt_username(char **username);
    esp_err_t um_nvs_get_mqtt_password(char **password);

    /* MQTT Setters */
    esp_err_t um_nvs_set_mqtt_enabled(bool enabled);
    esp_err_t um_nvs_set_mqtt_host(const char *host);
    esp_err_t um_nvs_set_mqtt_port(uint16_t port);
    esp_err_t um_nvs_set_mqtt_username(const char *username);
    esp_err_t um_nvs_set_mqtt_password(const char *password);

    /* Webhooks Getters */
    esp_err_t um_nvs_get_webhooks_enabled(bool *enabled);
    esp_err_t um_nvs_get_webhooks_url(char **url);

    /* Webhooks Setters */
    esp_err_t um_nvs_set_webhooks_enabled(bool enabled);
    esp_err_t um_nvs_set_webhooks_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* UM_NVS_H */