#include "um_ethernet.h"
#include "um_events.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ethernet_basic";
static esp_netif_t *eth_netif = NULL;
static esp_eth_handle_t *eth_handles = NULL;
static uint8_t eth_port_cnt = 0;
static um_eth_config_t current_config = {.mode = UM_ETH_MODE_DHCP};
static bool is_initialized = false;

static void apply_ip_config(void)
{
    if (!eth_netif)
        return;

    if (current_config.mode == UM_ETH_MODE_STATIC)
    {
        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = inet_addr(current_config.ip);
        ip_info.netmask.addr = inet_addr(current_config.netmask);
        ip_info.gw.addr = inet_addr(current_config.gateway);

        esp_netif_dhcpc_stop(eth_netif);
        esp_netif_set_ip_info(eth_netif, &ip_info);

        ESP_LOGI(TAG, "Static IP configured: %s", current_config.ip);
    }
    else
    {
        esp_netif_dhcpc_start(eth_netif);
        ESP_LOGI(TAG, "DHCP enabled");
    }
}

static void eth_disconnect_event_handler(void *handler_args, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data)
{
    ESP_LOGW(TAG, "Ethernet disconnected");
    um_event_publish(UMNI_EVENT_ETH_DISCONNECTED, NULL, 0, portMAX_DELAY);
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "MASK: " IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "GW: " IPSTR, IP2STR(&ip_info->gw));

    um_event_publish(UMNI_EVENT_ETH_CONNECTED, NULL, 0, portMAX_DELAY);
}

static void init_internal(um_eth_config_t *config)
{
    esp_err_t res;

    // Save config if provided
    if (config)
    {
        current_config = *config;
    }

    // Init network stack
    ESP_ERROR_CHECK(esp_netif_init());
    res = esp_event_loop_create_default();
    if (res == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "Event bus already initialized");
    }

    // Init Ethernet driver
    ESP_ERROR_CHECK(ethernet_init_all(&eth_handles, &eth_port_cnt));

    // Create netif
    if (eth_port_cnt > 0)
    {
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        eth_netif = esp_netif_new(&cfg);
        ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handles[0])));
    }

    // Register handlers
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &eth_disconnect_event_handler, NULL));

    // Apply IP config before start
    apply_ip_config();

    // Start Ethernet
    for (int i = 0; i < eth_port_cnt; i++)
    {
        ESP_ERROR_CHECK(esp_eth_start(eth_handles[i]));
    }

    is_initialized = true;
}

void um_ethernet_init(um_eth_config_t *config)
{
    init_internal(config);
}

void um_ethernet_reinit(um_eth_config_t *config)
{
    if (!is_initialized)
    {
        um_ethernet_init(config);
        return;
    }

    ESP_LOGI(TAG, "Reinitializing Ethernet...");

    // Stop and cleanup
    for (int i = 0; i < eth_port_cnt; i++)
    {
        esp_eth_stop(eth_handles[i]);
    }

    if (eth_netif)
    {
        esp_netif_destroy(eth_netif);
        eth_netif = NULL;
    }

    // Unregister handlers
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler);
    esp_event_handler_unregister(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &eth_disconnect_event_handler);

    // Reinit with new config
    init_internal(config);
}

char *um_ethernet_get_ip(void)
{
    if (!eth_netif)
        return NULL;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(eth_netif, &ip_info) != ESP_OK)
    {
        return NULL;
    }

    char *ip_str = malloc(16);
    if (ip_str)
    {
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    }
    return ip_str;
}