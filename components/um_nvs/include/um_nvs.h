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
extern "C" {
#endif

/* NVS Key Definitions */
#define UM_NVS_KEY_INSTALLED          "inst"
#define UM_NVS_KEY_HOSTNAME           "name"
#define UM_NVS_KEY_MACNAME            "macname"
#define UM_NVS_KEY_USERNAME           "admusr"
#define UM_NVS_KEY_PASSWORD           "admpwd"
#define UM_NVS_KEY_NTP                "ntp"
#define UM_NVS_KEY_UPDATES_CHANNEL    "upd"
#define UM_NVS_KEY_TIMEZONE           "tz"
#define UM_NVS_KEY_POWERON_AT         "poweron"
#define UM_NVS_KEY_RESET_AT           "resetat"
#define UM_NVS_KEY_NETWORK_MODE       "nwmode"
#define UM_NVS_KEY_WIFI_STA_MAC       "stmac"
#define UM_NVS_KEY_WIFI_STA_SSID      "stname"
#define UM_NVS_KEY_WIFI_STA_PWD       "stpwd"
#define UM_NVS_KEY_WIFI_MAC           "wmac"
#define UM_NVS_KEY_WIFI_TYPE          "wt"
#define UM_NVS_KEY_WIFI_IP            "wip"
#define UM_NVS_KEY_WIFI_NETMASK       "wnm"
#define UM_NVS_KEY_WIFI_GATEWAY       "wgw"
#define UM_NVS_KEY_WIFI_DNS           "wdns"
#define UM_NVS_KEY_ETH_MAC            "emac"
#define UM_NVS_KEY_ETH_TYPE           "et"
#define UM_NVS_KEY_ETH_IP             "eip"
#define UM_NVS_KEY_ETH_NETMASK        "enm"
#define UM_NVS_KEY_ETH_GATEWAY        "egw"
#define UM_NVS_KEY_ETH_DNS            "edns"
#define UM_NVS_KEY_OT_EN              "oten"
#define UM_NVS_KEY_OT_CH              "otch"
#define UM_NVS_KEY_OT_CH2             "otch2"
#define UM_NVS_KEY_OT_CH_SETPOINT     "ottbsp"
#define UM_NVS_KEY_OT_DHW_SETPOINT    "otdhwsp"
#define UM_NVS_KEY_OT_DHW             "otdhw"
#define UM_NVS_KEY_OT_COOL            "otcol"
#define UM_NVS_KEY_OT_MOD             "otmod"
#define UM_NVS_KEY_OT_OTC             "ototc"
#define UM_NVS_KEY_OT_HCR             "othcr"
#define UM_NVS_KEY_OUTPUTS_DATA       "outputs"
#define UM_NVS_KEY_MQTT_ENABLED       "mqen"
#define UM_NVS_KEY_MQTT_HOST          "mqhost"
#define UM_NVS_KEY_MQTT_PORT          "mqport"
#define UM_NVS_KEY_MQTT_USER          "mquser"
#define UM_NVS_KEY_MQTT_PWD           "mqpwd"
#define UM_NVS_KEY_WEBHOOKS           "whk"
#define UM_NVS_KEY_WEBHOOKS_URL       "whkurl"

/* Default Values */
#define UM_NVS_DEFAULT_NTP                "0.ru.pool.ntp.org"
#define UM_NVS_DEFAULT_TIMEZONE           "MSK-3"
#define UM_NVS_DEFAULT_NETWORK_MODE       1
#define UM_NVS_DEFAULT_WIFI_TYPE          1
#define UM_NVS_DEFAULT_ETH_TYPE           1
#define UM_NVS_DEFAULT_OT_EN              0
#define UM_NVS_DEFAULT_OT_CH              1
#define UM_NVS_DEFAULT_OT_CH2             0
#define UM_NVS_DEFAULT_OT_CH_SETPOINT     45
#define UM_NVS_DEFAULT_OT_DHW_SETPOINT    60
#define UM_NVS_DEFAULT_OT_DHW             1
#define UM_NVS_DEFAULT_OT_COOL            0
#define UM_NVS_DEFAULT_OT_MOD             99
#define UM_NVS_DEFAULT_OT_OTC             0
#define UM_NVS_DEFAULT_OUTPUTS_DATA       0
#define UM_NVS_DEFAULT_MQTT_ENABLED       0
#define UM_NVS_DEFAULT_MQTT_PORT          1883
#define UM_NVS_DEFAULT_WEBHOOKS           0

/* Network Mode Definitions */
#define UM_NVS_NETWORK_MODE_ETH       1
#define UM_NVS_NETWORK_MODE_WIFI_AP   2
#define UM_NVS_NETWORK_MODE_WIFI_STA  3

/* IP Type Definitions */
#define UM_NVS_IP_TYPE_DHCP           1
#define UM_NVS_IP_TYPE_STATIC         2

/**
 * @brief Initialize NVS storage
 * 
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t um_nvs_init(void);

/**
 * @brief Open NVS namespace for operations
 * 
 * @param namespace Namespace name
 * @return true Success
 * @return false Failure
 */
bool um_nvs_open(const char* namespace);

/**
 * @brief Close NVS namespace
 */
void um_nvs_close(void);

/**
 * @brief Check if system is installed
 * 
 * @return true System is installed
 * @return false System is not installed
 */
bool um_nvs_is_installed(void);

/**
 * @brief Erase all data in current namespace
 * 
 * @return true Success
 * @return false Failure
 */
bool um_nvs_erase(void);

/**
 * @brief Erase specific key from NVS
 * 
 * @param key Key to erase
 * @return true Success
 * @return false Failure
 */
bool um_nvs_delete_key(const char* key);

/**
 * @brief Initialize NVS with default values
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_nvs_initialize_with_defaults(void);

/* Generic read functions */
int8_t um_nvs_read_i8(const char* key);
int16_t um_nvs_read_i16(const char* key);
int64_t um_nvs_read_i64(const char* key);
char* um_nvs_read_str(const char* key);
esp_err_t um_nvs_read_u16(const char* key, uint16_t* out);

/* Generic write functions */
esp_err_t um_nvs_write_i8(const char* key, int8_t value);
esp_err_t um_nvs_write_i16(const char* key, int16_t value);
esp_err_t um_nvs_write_u16(const char* key, uint16_t value);
bool um_nvs_write_str(const char* key, const char* value);
esp_err_t um_nvs_write_i64(const char* key, int64_t value);

/* System Getters */
bool um_nvs_get_installed(void);
char* um_nvs_get_hostname(void);
char* um_nvs_get_macname(void);
char* um_nvs_get_username(void);
char* um_nvs_get_password(void);
char* um_nvs_get_ntp(void);
uint8_t um_nvs_get_updates_channel(void);
char* um_nvs_get_timezone(void);
char* um_nvs_get_poweron_at(void);
char* um_nvs_get_reset_at(void);

/* System Setters */
bool um_nvs_set_installed(bool installed);
bool um_nvs_set_hostname(const char* hostname);
bool um_nvs_set_macname(const char* macname);
bool um_nvs_set_username(const char* username);
bool um_nvs_set_password(const char* password);
bool um_nvs_set_ntp(const char* ntp);
bool um_nvs_set_updates_channel(uint8_t channel);
bool um_nvs_set_timezone(const char* timezone);
bool um_nvs_set_poweron_at(const char* poweron_at);
bool um_nvs_set_reset_at(const char* reset_at);

/* Network Getters */
uint8_t um_nvs_get_network_mode(void);
char* um_nvs_get_wifi_sta_mac(void);
char* um_nvs_get_wifi_sta_ssid(void);
char* um_nvs_get_wifi_sta_password(void);
char* um_nvs_get_wifi_mac(void);
uint8_t um_nvs_get_wifi_type(void);
char* um_nvs_get_wifi_ip(void);
char* um_nvs_get_wifi_netmask(void);
char* um_nvs_get_wifi_gateway(void);
char* um_nvs_get_wifi_dns(void);
char* um_nvs_get_eth_mac(void);
uint8_t um_nvs_get_eth_type(void);
char* um_nvs_get_eth_ip(void);
char* um_nvs_get_eth_netmask(void);
char* um_nvs_get_eth_gateway(void);
char* um_nvs_get_eth_dns(void);

/* Network Setters */
bool um_nvs_set_network_mode(uint8_t mode);
bool um_nvs_set_wifi_sta_mac(const char* mac);
bool um_nvs_set_wifi_sta_ssid(const char* ssid);
bool um_nvs_set_wifi_sta_password(const char* password);
bool um_nvs_set_wifi_mac(const char* mac);
bool um_nvs_set_wifi_type(uint8_t type);
bool um_nvs_set_wifi_ip(const char* ip);
bool um_nvs_set_wifi_netmask(const char* netmask);
bool um_nvs_set_wifi_gateway(const char* gateway);
bool um_nvs_set_wifi_dns(const char* dns);
bool um_nvs_set_eth_mac(const char* mac);
bool um_nvs_set_eth_type(uint8_t type);
bool um_nvs_set_eth_ip(const char* ip);
bool um_nvs_set_eth_netmask(const char* netmask);
bool um_nvs_set_eth_gateway(const char* gateway);
bool um_nvs_set_eth_dns(const char* dns);

/* OpenTherm Getters */
bool um_nvs_get_ot_enabled(void);
bool um_nvs_get_ot_ch_enabled(void);
bool um_nvs_get_ot_ch2_enabled(void);
uint8_t um_nvs_get_ot_ch_setpoint(void);
uint8_t um_nvs_get_ot_dhw_setpoint(void);
bool um_nvs_get_ot_dhw_enabled(void);
bool um_nvs_get_ot_cool_enabled(void);
uint8_t um_nvs_get_ot_modulation(void);
bool um_nvs_get_ot_outdoor_temp_comp(void);
uint8_t um_nvs_get_ot_heating_curve_ratio(void);

/* OpenTherm Setters */
bool um_nvs_set_ot_enabled(bool enabled);
bool um_nvs_set_ot_ch_enabled(bool enabled);
bool um_nvs_set_ot_ch2_enabled(bool enabled);
bool um_nvs_set_ot_ch_setpoint(uint8_t setpoint);
bool um_nvs_set_ot_dhw_setpoint(uint8_t setpoint);
bool um_nvs_set_ot_dhw_enabled(bool enabled);
bool um_nvs_set_ot_cool_enabled(bool enabled);
bool um_nvs_set_ot_modulation(uint8_t modulation);
bool um_nvs_set_ot_outdoor_temp_comp(bool enabled);
bool um_nvs_set_ot_heating_curve_ratio(uint8_t ratio);

/* Outputs Getters */
uint8_t um_nvs_get_outputs_data(void);

/* Outputs Setters */
bool um_nvs_set_outputs_data(uint8_t data);

/* MQTT Getters */
bool um_nvs_get_mqtt_enabled(void);
char* um_nvs_get_mqtt_host(void);
uint16_t um_nvs_get_mqtt_port(void);
char* um_nvs_get_mqtt_username(void);
char* um_nvs_get_mqtt_password(void);

/* MQTT Setters */
bool um_nvs_set_mqtt_enabled(bool enabled);
bool um_nvs_set_mqtt_host(const char* host);
bool um_nvs_set_mqtt_port(uint16_t port);
bool um_nvs_set_mqtt_username(const char* username);
bool um_nvs_set_mqtt_password(const char* password);

/* Webhooks Getters */
bool um_nvs_get_webhooks_enabled(void);
char* um_nvs_get_webhooks_url(void);

/* Webhooks Setters */
bool um_nvs_set_webhooks_enabled(bool enabled);
bool um_nvs_set_webhooks_url(const char* url);

#ifdef __cplusplus
}
#endif

#endif /* UM_NVS_H */