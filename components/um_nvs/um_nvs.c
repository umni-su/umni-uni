/**
 * @file um_nvs.c
 * @brief Non-volatile storage management implementation
 * @version 1.0.0
 */

#include <stdlib.h>
#include <string.h>
#include "um_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"

static const char* TAG = "nvs";

static nvs_handle_t um_nvs_handle = 0;
static char* current_namespace = NULL;

/* Forward declarations */
static esp_err_t commit_changes(void);

/**
 * @brief Initialize NVS flash storage
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(err));
        return err;
    }

    um_nvs_open("um_nvs");
    
    ESP_LOGI(TAG, "NVS initialized successfully");
    return ESP_OK;
}

/**
 * @brief Open NVS namespace
 * 
 * @param namespace Namespace to open
 * @return true Success
 * @return false Failure
 */
bool um_nvs_open(const char* namespace)
{
    if (namespace == NULL) {
        ESP_LOGE(TAG, "Namespace cannot be NULL");
        return false;
    }
    
    esp_err_t err = nvs_open(namespace, NVS_READWRITE, &um_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", namespace, esp_err_to_name(err));
        return false;
    }
    
    current_namespace = strdup(namespace);
    ESP_LOGI(TAG, "Opened NVS namespace: %s", namespace);
    return true;
}

/**
 * @brief Close NVS namespace
 */
void um_nvs_close(void)
{
    if (um_nvs_handle != 0) {
        nvs_close(um_nvs_handle);
        um_nvs_handle = 0;
    }
    
    if (current_namespace != NULL) {
        free(current_namespace);
        current_namespace = NULL;
    }
    
    ESP_LOGI(TAG, "NVS namespace closed");
}

/**
 * @brief Check if system is installed
 * 
 * @return true System is installed
 * @return false System is not installed
 */
bool um_nvs_is_installed(void)
{
    int8_t installed = um_nvs_read_i8(UM_NVS_KEY_INSTALLED);
    char* username = um_nvs_get_username();
    char* password = um_nvs_get_password();
    
    bool result = (installed == 1) && (username != NULL) && (password != NULL);
    
    if (username != NULL) free(username);
    if (password != NULL) free(password);
    
    return result;
}

/**
 * @brief Erase all data in current namespace
 * 
 * @return true Success
 * @return false Failure
 */
bool um_nvs_erase(void)
{
    if (um_nvs_handle == 0) {
        ESP_LOGE(TAG, "NVS not opened");
        return false;
    }
    
    esp_err_t err = nvs_erase_all(um_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    err = commit_changes();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit erase: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "NVS erased successfully");
    return true;
}

/**
 * @brief Delete specific key from NVS
 * 
 * @param key Key to delete
 * @return true Success
 * @return false Failure
 */
bool um_nvs_delete_key(const char* key)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return false;
    }
    
    esp_err_t err = nvs_erase_key(um_nvs_handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete key '%s': %s", key, esp_err_to_name(err));
        return false;
    }
    
    err = commit_changes();
    return err == ESP_OK;
}

/**
 * @brief Commit changes to NVS
 * 
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t commit_changes(void)
{
    if (um_nvs_handle == 0) {
        return ESP_FAIL;
    }
    
    esp_err_t err = nvs_commit(um_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief Initialize NVS with default values
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t um_nvs_initialize_with_defaults(void)
{
    esp_err_t res = ESP_OK;
    
    res = um_nvs_write_i8(UM_NVS_KEY_ETH_TYPE, UM_NVS_IP_TYPE_DHCP);
    res = um_nvs_write_i8(UM_NVS_KEY_WIFI_TYPE, UM_NVS_IP_TYPE_DHCP);
    res = um_nvs_write_i8(UM_NVS_KEY_UPDATES_CHANNEL, 1);
    res = um_nvs_write_str(UM_NVS_KEY_NTP, UM_NVS_DEFAULT_NTP);
    res = um_nvs_write_str(UM_NVS_KEY_TIMEZONE, UM_NVS_DEFAULT_TIMEZONE);
    
    /* OpenTherm defaults */
    res = um_nvs_write_i8(UM_NVS_KEY_OT_CH, UM_NVS_DEFAULT_OT_CH);
    res = um_nvs_write_i8(UM_NVS_KEY_OT_CH_SETPOINT, UM_NVS_DEFAULT_OT_CH_SETPOINT);
    res = um_nvs_write_i8(UM_NVS_KEY_OT_DHW_SETPOINT, UM_NVS_DEFAULT_OT_DHW_SETPOINT);
    res = um_nvs_write_i8(UM_NVS_KEY_OT_DHW, UM_NVS_DEFAULT_OT_DHW);
    
    /* Network defaults */
    res = um_nvs_write_i8(UM_NVS_KEY_NETWORK_MODE, UM_NVS_DEFAULT_NETWORK_MODE);
    
    /* MQTT defaults */
    res = um_nvs_write_u16(UM_NVS_KEY_MQTT_PORT, UM_NVS_DEFAULT_MQTT_PORT);
    
    ESP_LOGI(TAG, "NVS initialized with default values");
    return res;
}

/* Generic read functions */
int8_t um_nvs_read_i8(const char* key)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return -1;
    }
    
    int8_t value = 0;
    esp_err_t err = nvs_get_i8(um_nvs_handle, key, &value);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return -1;
    }
    
    ESP_LOGD(TAG, "Read i8: %s = %d", key, value);
    return value;
}

int16_t um_nvs_read_i16(const char* key)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return -1;
    }
    
    int16_t value = 0;
    esp_err_t err = nvs_get_i16(um_nvs_handle, key, &value);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return -1;
    }
    
    ESP_LOGD(TAG, "Read i16: %s = %d", key, value);
    return value;
}

int64_t um_nvs_read_i64(const char* key)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return -1;
    }
    
    int64_t value = 0;
    esp_err_t err = nvs_get_i64(um_nvs_handle, key, &value);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return -1;
    }
    
    ESP_LOGD(TAG, "Read i64: %s = %lld", key, value);
    return value;
}

char* um_nvs_read_str(const char* key)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return NULL;
    }
    
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(um_nvs_handle, key, NULL, &required_size);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return NULL;
    }
    
    if (required_size == 0) {
        return NULL;
    }
    
    char* value = malloc(required_size);
    if (value == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for string");
        return NULL;
    }
    
    err = nvs_get_str(um_nvs_handle, key, value, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read string '%s': %s", key, esp_err_to_name(err));
        free(value);
        return NULL;
    }
    
    ESP_LOGD(TAG, "Read str: %s = %s", key, value);
    return value;
}

esp_err_t um_nvs_read_u16(const char* key, uint16_t* out)
{
    if (um_nvs_handle == 0 || key == NULL || out == NULL) {
        return ESP_FAIL;
    }
    
    esp_err_t err = nvs_get_u16(um_nvs_handle, key, out);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Key '%s' not found: %s", key, esp_err_to_name(err));
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Read u16: %s = %u", key, *out);
    return ESP_OK;
}

/* Generic write functions */
esp_err_t um_nvs_write_i8(const char* key, int8_t value)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return ESP_FAIL;
    }
    
    esp_err_t err = nvs_set_i8(um_nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write i8 '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    
    err = commit_changes();
    ESP_LOGD(TAG, "Write i8: %s = %d", key, value);
    return err;
}

esp_err_t um_nvs_write_i16(const char* key, int16_t value)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return ESP_FAIL;
    }
    
    esp_err_t err = nvs_set_i16(um_nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write i16 '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    
    err = commit_changes();
    ESP_LOGD(TAG, "Write i16: %s = %d", key, value);
    return err;
}

esp_err_t um_nvs_write_u16(const char* key, uint16_t value)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return ESP_FAIL;
    }
    
    esp_err_t err = nvs_set_u16(um_nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write u16 '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    
    err = commit_changes();
    ESP_LOGD(TAG, "Write u16: %s = %u", key, value);
    return err;
}

bool um_nvs_write_str(const char* key, const char* value)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return false;
    }
    
    if (value == NULL) {
        /* Delete the key if value is NULL */
        return um_nvs_delete_key(key);
    }
    
    esp_err_t err = nvs_set_str(um_nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write str '%s': %s", key, esp_err_to_name(err));
        return false;
    }
    
    err = commit_changes();
    ESP_LOGD(TAG, "Write str: %s = %s", key, value);
    return err == ESP_OK;
}

esp_err_t um_nvs_write_i64(const char* key, int64_t value)
{
    if (um_nvs_handle == 0 || key == NULL) {
        return ESP_FAIL;
    }
    
    esp_err_t err = nvs_set_i64(um_nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write i64 '%s': %s", key, esp_err_to_name(err));
        return err;
    }
    
    err = commit_changes();
    ESP_LOGD(TAG, "Write i64: %s = %lld", key, value);
    return err;
}

/* System Getters */
bool um_nvs_get_installed(void) { return um_nvs_read_i8(UM_NVS_KEY_INSTALLED) == 1; }
char* um_nvs_get_hostname(void) { return um_nvs_read_str(UM_NVS_KEY_HOSTNAME); }
char* um_nvs_get_macname(void) { return um_nvs_read_str(UM_NVS_KEY_MACNAME); }
char* um_nvs_get_username(void) { return um_nvs_read_str(UM_NVS_KEY_USERNAME); }
char* um_nvs_get_password(void) { return um_nvs_read_str(UM_NVS_KEY_PASSWORD); }
char* um_nvs_get_ntp(void) { return um_nvs_read_str(UM_NVS_KEY_NTP); }
uint8_t um_nvs_get_updates_channel(void) { return um_nvs_read_i8(UM_NVS_KEY_UPDATES_CHANNEL); }
char* um_nvs_get_timezone(void) { return um_nvs_read_str(UM_NVS_KEY_TIMEZONE); }
char* um_nvs_get_poweron_at(void) { return um_nvs_read_str(UM_NVS_KEY_POWERON_AT); }
char* um_nvs_get_reset_at(void) { return um_nvs_read_str(UM_NVS_KEY_RESET_AT); }

/* System Setters */
bool um_nvs_set_installed(bool installed) { return um_nvs_write_i8(UM_NVS_KEY_INSTALLED, installed ? 1 : 0) == ESP_OK; }
bool um_nvs_set_hostname(const char* hostname) { return um_nvs_write_str(UM_NVS_KEY_HOSTNAME, hostname); }
bool um_nvs_set_macname(const char* macname) { return um_nvs_write_str(UM_NVS_KEY_MACNAME, macname); }
bool um_nvs_set_username(const char* username) { return um_nvs_write_str(UM_NVS_KEY_USERNAME, username); }
bool um_nvs_set_password(const char* password) { return um_nvs_write_str(UM_NVS_KEY_PASSWORD, password); }
bool um_nvs_set_ntp(const char* ntp) { return um_nvs_write_str(UM_NVS_KEY_NTP, ntp); }
bool um_nvs_set_updates_channel(uint8_t channel) { return um_nvs_write_i8(UM_NVS_KEY_UPDATES_CHANNEL, channel) == ESP_OK; }
bool um_nvs_set_timezone(const char* timezone) { return um_nvs_write_str(UM_NVS_KEY_TIMEZONE, timezone); }
bool um_nvs_set_poweron_at(const char* poweron_at) { return um_nvs_write_str(UM_NVS_KEY_POWERON_AT, poweron_at); }
bool um_nvs_set_reset_at(const char* reset_at) { return um_nvs_write_str(UM_NVS_KEY_RESET_AT, reset_at); }

/* Network Getters */
uint8_t um_nvs_get_network_mode(void) { return um_nvs_read_i8(UM_NVS_KEY_NETWORK_MODE); }
char* um_nvs_get_wifi_sta_mac(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_STA_MAC); }
char* um_nvs_get_wifi_sta_ssid(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_STA_SSID); }
char* um_nvs_get_wifi_sta_password(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_STA_PWD); }
char* um_nvs_get_wifi_mac(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_MAC); }
uint8_t um_nvs_get_wifi_type(void) { return um_nvs_read_i8(UM_NVS_KEY_WIFI_TYPE); }
char* um_nvs_get_wifi_ip(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_IP); }
char* um_nvs_get_wifi_netmask(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_NETMASK); }
char* um_nvs_get_wifi_gateway(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_GATEWAY); }
char* um_nvs_get_wifi_dns(void) { return um_nvs_read_str(UM_NVS_KEY_WIFI_DNS); }
char* um_nvs_get_eth_mac(void) { return um_nvs_read_str(UM_NVS_KEY_ETH_MAC); }
uint8_t um_nvs_get_eth_type(void) { return um_nvs_read_i8(UM_NVS_KEY_ETH_TYPE); }
char* um_nvs_get_eth_ip(void) { return um_nvs_read_str(UM_NVS_KEY_ETH_IP); }
char* um_nvs_get_eth_netmask(void) { return um_nvs_read_str(UM_NVS_KEY_ETH_NETMASK); }
char* um_nvs_get_eth_gateway(void) { return um_nvs_read_str(UM_NVS_KEY_ETH_GATEWAY); }
char* um_nvs_get_eth_dns(void) { return um_nvs_read_str(UM_NVS_KEY_ETH_DNS); }

/* Network Setters */
bool um_nvs_set_network_mode(uint8_t mode) { return um_nvs_write_i8(UM_NVS_KEY_NETWORK_MODE, mode) == ESP_OK; }
bool um_nvs_set_wifi_sta_mac(const char* mac) { return um_nvs_write_str(UM_NVS_KEY_WIFI_STA_MAC, mac); }
bool um_nvs_set_wifi_sta_ssid(const char* ssid) { return um_nvs_write_str(UM_NVS_KEY_WIFI_STA_SSID, ssid); }
bool um_nvs_set_wifi_sta_password(const char* password) { return um_nvs_write_str(UM_NVS_KEY_WIFI_STA_PWD, password); }
bool um_nvs_set_wifi_mac(const char* mac) { return um_nvs_write_str(UM_NVS_KEY_WIFI_MAC, mac); }
bool um_nvs_set_wifi_type(uint8_t type) { return um_nvs_write_i8(UM_NVS_KEY_WIFI_TYPE, type) == ESP_OK; }
bool um_nvs_set_wifi_ip(const char* ip) { return um_nvs_write_str(UM_NVS_KEY_WIFI_IP, ip); }
bool um_nvs_set_wifi_netmask(const char* netmask) { return um_nvs_write_str(UM_NVS_KEY_WIFI_NETMASK, netmask); }
bool um_nvs_set_wifi_gateway(const char* gateway) { return um_nvs_write_str(UM_NVS_KEY_WIFI_GATEWAY, gateway); }
bool um_nvs_set_wifi_dns(const char* dns) { return um_nvs_write_str(UM_NVS_KEY_WIFI_DNS, dns); }
bool um_nvs_set_eth_mac(const char* mac) { return um_nvs_write_str(UM_NVS_KEY_ETH_MAC, mac); }
bool um_nvs_set_eth_type(uint8_t type) { return um_nvs_write_i8(UM_NVS_KEY_ETH_TYPE, type) == ESP_OK; }
bool um_nvs_set_eth_ip(const char* ip) { return um_nvs_write_str(UM_NVS_KEY_ETH_IP, ip); }
bool um_nvs_set_eth_netmask(const char* netmask) { return um_nvs_write_str(UM_NVS_KEY_ETH_NETMASK, netmask); }
bool um_nvs_set_eth_gateway(const char* gateway) { return um_nvs_write_str(UM_NVS_KEY_ETH_GATEWAY, gateway); }
bool um_nvs_set_eth_dns(const char* dns) { return um_nvs_write_str(UM_NVS_KEY_ETH_DNS, dns); }

/* OpenTherm Getters */
bool um_nvs_get_ot_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_EN) == 1; }
bool um_nvs_get_ot_ch_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_CH) == 1; }
bool um_nvs_get_ot_ch2_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_CH2) == 1; }
uint8_t um_nvs_get_ot_ch_setpoint(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_CH_SETPOINT); }
uint8_t um_nvs_get_ot_dhw_setpoint(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_DHW_SETPOINT); }
bool um_nvs_get_ot_dhw_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_DHW) == 1; }
bool um_nvs_get_ot_cool_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_COOL) == 1; }
uint8_t um_nvs_get_ot_modulation(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_MOD); }
bool um_nvs_get_ot_outdoor_temp_comp(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_OTC) == 1; }
uint8_t um_nvs_get_ot_heating_curve_ratio(void) { return um_nvs_read_i8(UM_NVS_KEY_OT_HCR); }

/* OpenTherm Setters */
bool um_nvs_set_ot_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_OT_EN, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_ot_ch_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_OT_CH, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_ot_ch2_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_OT_CH2, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_ot_ch_setpoint(uint8_t setpoint) { return um_nvs_write_i8(UM_NVS_KEY_OT_CH_SETPOINT, setpoint) == ESP_OK; }
bool um_nvs_set_ot_dhw_setpoint(uint8_t setpoint) { return um_nvs_write_i8(UM_NVS_KEY_OT_DHW_SETPOINT, setpoint) == ESP_OK; }
bool um_nvs_set_ot_dhw_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_OT_DHW, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_ot_cool_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_OT_COOL, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_ot_modulation(uint8_t modulation) { return um_nvs_write_i8(UM_NVS_KEY_OT_MOD, modulation) == ESP_OK; }
bool um_nvs_set_ot_outdoor_temp_comp(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_OT_OTC, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_ot_heating_curve_ratio(uint8_t ratio) { return um_nvs_write_i8(UM_NVS_KEY_OT_HCR, ratio) == ESP_OK; }

/* Outputs Getters */
uint8_t um_nvs_get_outputs_data(void) { return um_nvs_read_i8(UM_NVS_KEY_OUTPUTS_DATA); }

/* Outputs Setters */
bool um_nvs_set_outputs_data(uint8_t data) { return um_nvs_write_i8(UM_NVS_KEY_OUTPUTS_DATA, data) == ESP_OK; }

/* MQTT Getters */
bool um_nvs_get_mqtt_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_MQTT_ENABLED) == 1; }
char* um_nvs_get_mqtt_host(void) { return um_nvs_read_str(UM_NVS_KEY_MQTT_HOST); }
uint16_t um_nvs_get_mqtt_port(void) { 
    uint16_t port = 0;
    um_nvs_read_u16(UM_NVS_KEY_MQTT_PORT, &port);
    return port;
}
char* um_nvs_get_mqtt_username(void) { return um_nvs_read_str(UM_NVS_KEY_MQTT_USER); }
char* um_nvs_get_mqtt_password(void) { return um_nvs_read_str(UM_NVS_KEY_MQTT_PWD); }

/* MQTT Setters */
bool um_nvs_set_mqtt_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_MQTT_ENABLED, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_mqtt_host(const char* host) { return um_nvs_write_str(UM_NVS_KEY_MQTT_HOST, host); }
bool um_nvs_set_mqtt_port(uint16_t port) { return um_nvs_write_u16(UM_NVS_KEY_MQTT_PORT, port) == ESP_OK; }
bool um_nvs_set_mqtt_username(const char* username) { return um_nvs_write_str(UM_NVS_KEY_MQTT_USER, username); }
bool um_nvs_set_mqtt_password(const char* password) { return um_nvs_write_str(UM_NVS_KEY_MQTT_PWD, password); }

/* Webhooks Getters */
bool um_nvs_get_webhooks_enabled(void) { return um_nvs_read_i8(UM_NVS_KEY_WEBHOOKS) == 1; }
char* um_nvs_get_webhooks_url(void) { return um_nvs_read_str(UM_NVS_KEY_WEBHOOKS_URL); }

/* Webhooks Setters */
bool um_nvs_set_webhooks_enabled(bool enabled) { return um_nvs_write_i8(UM_NVS_KEY_WEBHOOKS, enabled ? 1 : 0) == ESP_OK; }
bool um_nvs_set_webhooks_url(const char* url) { return um_nvs_write_str(UM_NVS_KEY_WEBHOOKS_URL, url); }