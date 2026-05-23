#include "um_network.h"
#include "esp_log.h"
#include "um_dio.h"
#include "um_nvs.h"
#include "base_config.h"
#if UM_FEATURE_ENABLED(ETHERNET)
#include "um_ethernet.h"
#endif
#if UM_FEATURE_ENABLED(WIFI)
#include "um_wifi.h"
#endif

#include <string.h>
#include <stdlib.h>

static const char *TAG = "network";

uint8_t network_mode = 0;
um_eth_config_t eth_config;
um_wifi_config_t wifi_config;

static void load_wifi_sta_config_from_nvs(um_wifi_config_t *config)
{
    char *ssid = NULL;
    char *password = NULL;
    uint8_t ip_type = 0;

    // Получаем hostname из NVS (он же будет SSID)
    um_nvs_get_wifi_sta_ssid(&ssid);
    um_nvs_get_wifi_sta_password(&password);
    um_nvs_get_wifi_type(&ip_type);

    // Используем hostname как SSID
    if (ssid && strlen(ssid) > 0)
    {
        strncpy(config->sta.ssid, ssid, sizeof(config->sta.ssid) - 1);
        config->sta.ssid[sizeof(config->sta.ssid) - 1] = '\0';
    }
    else
    {
        // Если hostname нет, используем дефолтный
        ESP_LOGW(TAG, "No ssid in NVS, using default SSID");
        strcpy(config->sta.ssid, "UMNI_Wifi");
    }

    // Копируем пароль
    strncpy(config->sta.password, password ? password : "", sizeof(config->sta.password) - 1);
    config->sta.password[sizeof(config->sta.password) - 1] = '\0';

    // Настройка IP (без изменений)
    if (ip_type == UM_NVS_IP_TYPE_STATIC)
    {
        config->ip_mode = UM_WIFI_IP_STATIC;

        char *ip = NULL, *netmask = NULL, *gateway = NULL, *dns = NULL;
        um_nvs_get_wifi_ip(&ip);
        um_nvs_get_wifi_netmask(&netmask);
        um_nvs_get_wifi_gateway(&gateway);
        um_nvs_get_wifi_dns(&dns);

        strncpy(config->ip, ip ? ip : "192.168.1.100", sizeof(config->ip) - 1);
        strncpy(config->netmask, netmask ? netmask : "255.255.255.0", sizeof(config->netmask) - 1);
        strncpy(config->gateway, gateway ? gateway : "192.168.1.1", sizeof(config->gateway) - 1);
        strncpy(config->dns, dns ? dns : "8.8.8.8", sizeof(config->dns) - 1);

        config->ip[sizeof(config->ip) - 1] = '\0';
        config->netmask[sizeof(config->netmask) - 1] = '\0';
        config->gateway[sizeof(config->gateway) - 1] = '\0';
        config->dns[sizeof(config->dns) - 1] = '\0';

        free(ip);
        free(netmask);
        free(gateway);
        free(dns);
    }
    else
    {
        config->ip_mode = UM_WIFI_IP_DHCP;
    }

    free(ssid);
    free(password);
}

static void load_wifi_ap_config_from_nvs(um_wifi_config_t *config)
{
    char *hostname = NULL;
    char *password = NULL;

    // Читаем SSID и пароль для AP (если есть)
    um_nvs_get_hostname(&hostname); // Используем те же ключи для AP
    um_nvs_get_wifi_sta_password(&password);

    if (hostname && strlen(hostname) > 0)
    {
        strncpy(config->ap.ssid, hostname, sizeof(config->ap.ssid) - 1);
        strncpy(config->ap.password, password ? password : "", sizeof(config->ap.password) - 1);
    }
    else
    {
        // Дефолтный SSID если не задан
        strcpy(config->ap.ssid, "UMNI_Config");
        strcpy(config->ap.password, "");
    }

    config->ap.ssid[sizeof(config->ap.ssid) - 1] = '\0';
    config->ap.password[sizeof(config->ap.password) - 1] = '\0';
    config->ap.channel = 6;
    config->ap.max_connections = 4;
    config->ap.hidden = false;

    free(hostname);
    free(password);
}

static void start_eth_with_nvs_config(void)
{
#if UM_FEATURE_ENABLED(ETHERNET)
    uint8_t eth_type = 0;
    um_nvs_get_eth_type(&eth_type);

    if (eth_type == UM_NVS_IP_TYPE_STATIC)
    {
        eth_config.mode = UM_ETH_MODE_STATIC;

        char *eth_ip = NULL, *eth_netmask = NULL, *eth_gateway = NULL, *eth_dns = NULL;
        um_nvs_get_eth_ip(&eth_ip);
        um_nvs_get_eth_netmask(&eth_netmask);
        um_nvs_get_eth_gateway(&eth_gateway);
        um_nvs_get_eth_dns(&eth_dns);

        strncpy(eth_config.ip, eth_ip ? eth_ip : "0.0.0.0", sizeof(eth_config.ip) - 1);
        strncpy(eth_config.netmask, eth_netmask ? eth_netmask : "255.255.255.0", sizeof(eth_config.netmask) - 1);
        strncpy(eth_config.gateway, eth_gateway ? eth_gateway : "0.0.0.0", sizeof(eth_config.gateway) - 1);
        strncpy(eth_config.dns, eth_dns ? eth_dns : "8.8.8.8", sizeof(eth_config.dns) - 1);

        eth_config.ip[sizeof(eth_config.ip) - 1] = '\0';
        eth_config.netmask[sizeof(eth_config.netmask) - 1] = '\0';
        eth_config.gateway[sizeof(eth_config.gateway) - 1] = '\0';
        eth_config.dns[sizeof(eth_config.dns) - 1] = '\0';

        free(eth_ip);
        free(eth_netmask);
        free(eth_gateway);
        free(eth_dns);
    }
    else
    {
        eth_config.mode = UM_ETH_MODE_DHCP;
    }

    um_ethernet_init(&eth_config);
    ESP_LOGI(TAG, "Ethernet started in mode: %s",
             eth_config.mode == UM_ETH_MODE_STATIC ? "STATIC" : "DHCP");
#endif
}

esp_err_t um_network_init(void)
{
    esp_err_t res = ESP_OK;
    bool config_state = false;

    // Состояние перемычки: true - режим конфигурации (перемычка снята/установлена?)
    // Уточним: config_state == false - перемычка установлена, режим конфигурации
    um_dio_get_config_state(&config_state);

    um_nvs_get_network_mode(&network_mode);

    if (network_mode == UM_NVS_NETWORK_MODE_NONE)
    {
        um_nvs_set_network_mode(UM_NVS_NETWORK_MODE_ETH);
        network_mode = UM_NVS_NETWORK_MODE_ETH;
    }

    // РЕЖИМ КОНФИГУРАЦИИ (перемычка установлена)
    if (!config_state)
    {
        ESP_LOGW(TAG, "CONFIG MODE: Ethernet + WiFi AP (192.168.4.1)");

#if UM_FEATURE_ENABLED(ETHERNET)
        start_eth_with_nvs_config();
#endif

#if UM_FEATURE_ENABLED(WIFI)
        // Запускаем WiFi AP для конфигурации
        memset(&wifi_config, 0, sizeof(wifi_config));
        wifi_config.mode = UM_WIFI_MODE_AP; // Меняем на AP!
        wifi_config.ip_mode = UM_WIFI_IP_STATIC;
        strcpy(wifi_config.ip, "192.168.4.1");
        strcpy(wifi_config.netmask, "255.255.255.0");
        strcpy(wifi_config.gateway, "192.168.4.1");
        strcpy(wifi_config.dns, "8.8.8.8");

        // Загружаем SSID/пароль из hostname для AP
        char *hostname = NULL;
        um_nvs_get_hostname(&hostname);
        if (hostname && strlen(hostname) > 0)
        {
            strcpy(wifi_config.ap.ssid, hostname);
        }
        else
        {
            strcpy(wifi_config.ap.ssid, "UMNI_Config");
        }
        strcpy(wifi_config.ap.password, ""); // Без пароля для простоты
        wifi_config.ap.channel = 6;
        wifi_config.ap.max_connections = 4;
        wifi_config.ap.hidden = false;

        free(hostname);

        um_wifi_init(&wifi_config);
        ESP_LOGI(TAG, "WiFi AP started on 192.168.4.1 with SSID: %s", wifi_config.ap.ssid);
#endif

        return ESP_OK;
    }

    // ШТАТНЫЙ РЕЖИМ (перемычка снята)
    ESP_LOGI(TAG, "NORMAL MODE: %s",
             network_mode == UM_NVS_NETWORK_MODE_ETH ? "Ethernet" : network_mode == UM_NVS_NETWORK_MODE_WIFI_AP ? "WiFi AP"
                                                                : network_mode == UM_NVS_NETWORK_MODE_WIFI_STA  ? "WiFi STA"
                                                                                                                : "Unknown");

    switch (network_mode)
    {
    case UM_NVS_NETWORK_MODE_ETH:
    {
#if UM_FEATURE_ENABLED(ETHERNET)
        start_eth_with_nvs_config();
#endif
        break;
    }

    case UM_NVS_NETWORK_MODE_WIFI_AP:
    {
#if UM_FEATURE_ENABLED(WIFI)
        memset(&wifi_config, 0, sizeof(wifi_config));
        wifi_config.mode = UM_WIFI_MODE_AP;
        wifi_config.ip_mode = UM_WIFI_IP_DHCP;

        load_wifi_ap_config_from_nvs(&wifi_config);

        um_wifi_init(&wifi_config);
        ESP_LOGI(TAG, "WiFi AP started with SSID: %s", wifi_config.ap.ssid);
#endif
        break;
    }

    case UM_NVS_NETWORK_MODE_WIFI_STA:
    {
#if UM_FEATURE_ENABLED(WIFI)
        memset(&wifi_config, 0, sizeof(wifi_config));
        wifi_config.mode = UM_WIFI_MODE_STA;

        load_wifi_sta_config_from_nvs(&wifi_config);

        um_wifi_init(&wifi_config);
        ESP_LOGI(TAG, "WiFi STA started, connecting to: %s with password %s", wifi_config.sta.ssid, wifi_config.sta.password);
#endif
        break;
    }

    default:
        ESP_LOGW(TAG, "Unknown network mode: %d", network_mode);
        return ESP_ERR_INVALID_ARG;
    }

    return res;
}

esp_err_t um_network_stop(void)
{
    esp_err_t res = ESP_OK;
    bool config_state = false;

    um_dio_get_config_state(&config_state);
    um_nvs_get_network_mode(&network_mode);

    // В режиме конфигурации останавливаем оба интерфейса
    if (!config_state)
    {
#if UM_FEATURE_ENABLED(ETHERNET)
        // TODO: добавить um_ethernet_stop() если нужен
#endif
#if UM_FEATURE_ENABLED(WIFI)
        um_wifi_reinit(NULL); // Переинициализация с NULL остановит WiFi
#endif
        return ESP_OK;
    }

    // Штатный режим
    switch (network_mode)
    {
    case UM_NVS_NETWORK_MODE_ETH:
    {
#if UM_FEATURE_ENABLED(ETHERNET)
        // TODO: добавить um_ethernet_stop() если нужен
#endif
        break;
    }
    case UM_NVS_NETWORK_MODE_WIFI_AP:
    case UM_NVS_NETWORK_MODE_WIFI_STA:
    {
#if UM_FEATURE_ENABLED(WIFI)
        um_wifi_reinit(NULL);
#endif
        break;
    }
    default:
        ESP_LOGW(TAG, "Unknown network mode: %d", network_mode);
        return ESP_ERR_INVALID_ARG;
    }

    return res;
}