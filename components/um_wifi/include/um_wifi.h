// um_wifi.h
#pragma once

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "lwip/inet.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        UM_WIFI_MODE_NULL = 0,
        UM_WIFI_MODE_STA = 1,
        UM_WIFI_MODE_AP = 2,
        UM_WIFI_MODE_APSTA = 3,
    } um_wifi_mode_t;

    typedef enum
    {
        UM_WIFI_IP_DHCP = 0,
        UM_WIFI_IP_STATIC = 1,
    } um_wifi_ip_mode_t;

    typedef struct
    {
        char ssid[64];
        char password[64];
        uint8_t channel;
        uint8_t max_connections;
        bool hidden;
    } um_wifi_ap_config_t;

    typedef struct
    {
        char ssid[64];
        char password[64];
        uint8_t timeout_s;
    } um_wifi_sta_config_t;

    typedef struct
    {
        um_wifi_mode_t mode;
        um_wifi_ip_mode_t ip_mode;
        char ip[16];
        char netmask[16];
        char gateway[16];
        char dns[16];
        um_wifi_sta_config_t sta;
        um_wifi_ap_config_t ap;
    } um_wifi_config_t;

    // Инициализация WiFi (NULL = режим NULL)
    void um_wifi_init(um_wifi_config_t *config);

    // Переинициализация с новой конфигурацией
    void um_wifi_reinit(um_wifi_config_t *config);

    // Отключение WiFi (останавливает текущие соединения)
    void um_wifi_disconnect(void);

    // Получить IP адрес STA (выделяет память, нужно освободить free())
    char *um_wifi_get_sta_ip(void);

    // Получить IP адрес AP (выделяет память, нужно освободить free())
    char *um_wifi_get_ap_ip(void);

    // Получить MAC адрес (строка, нужно освободить free())
    char *um_wifi_get_mac(um_wifi_mode_t iface);

    // Проверить, подключена ли STA к AP
    bool um_wifi_is_sta_connected(void);

    // Запустить сканирование сетей (выделяет память, нужно освободить free())
    wifi_ap_record_t *um_wifi_scan(uint16_t *count);

#ifdef __cplusplus
}
#endif