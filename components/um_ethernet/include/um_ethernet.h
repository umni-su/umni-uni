#pragma once
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ethernet_init.h"
#include "lwip/inet.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        UM_ETH_MODE_DHCP = 0,
        UM_ETH_MODE_STATIC = 1,
    } um_eth_mode_t;

    typedef struct
    {
        um_eth_mode_t mode;
        char ip[16];
        char netmask[16];
        char gateway[16];
        char dns[16];
    } um_eth_config_t;

    // Инициализация с конфигурацией (NULL = DHCP)
    void um_ethernet_init(um_eth_config_t *config);

    // Переинициализация с новой конфигурацией
    void um_ethernet_reinit(um_eth_config_t *config);

    // Получить текущий IP (выделяет память, нужно освободить free())
    char *um_ethernet_get_ip(void);

#ifdef __cplusplus
}
#endif