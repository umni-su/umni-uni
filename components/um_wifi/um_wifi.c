// um_wifi.c
#include "um_wifi.h"
#include "um_events.h"

static const char *TAG = "wifi_basic";
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;
static um_wifi_config_t current_config = {0};
static bool is_initialized = false;
static bool sta_connected = false;

#define MAC2STR_UM(mac) (mac)[0], (mac)[1], (mac)[2], (mac)[3], (mac)[4], (mac)[5]
#define MACSTR_UM "%02x:%02x:%02x:%02x:%02x:%02x"

// Обработчики для um_events
static void wifi_event_disconnected_handler(void *arg, esp_event_base_t event_base,
                                            int32_t event_id, void *event_data)
{
    uint8_t reason = 0;
    if (event_data != NULL)
    {
        wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;
        reason = disconnected->reason;
    }

    sta_connected = false;
    ESP_LOGW(TAG, "WiFi STA disconnected, reason: %d, retrying...", reason);
    um_event_publish(UMNI_EVENT_WIFI_DISCONNECTED, NULL, 0, portMAX_DELAY);
    esp_wifi_connect();
}

static void wifi_event_sta_start_handler(void *arg, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi STA started, connecting...");
    esp_wifi_connect();
}

static void wifi_event_sta_connected_handler(void *arg, esp_event_base_t event_base,
                                             int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi STA connected to AP");
}

static void wifi_event_ap_start_handler(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi AP started");
}

static void wifi_event_ap_stop_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi AP stopped");
}

static void wifi_event_ap_staconnected_handler(void *arg, esp_event_base_t event_base,
                                               int32_t event_id, void *event_data)
{
    if (event_data == NULL)
    {
        ESP_LOGD(TAG, "AP station connected event with NULL data");
        return;
    }

    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "Station " MACSTR_UM " joined", MAC2STR_UM(event->mac));
}

static void wifi_event_ap_stadisconnected_handler(void *arg, esp_event_base_t event_base,
                                                  int32_t event_id, void *event_data)
{
    if (event_data == NULL)
    {
        ESP_LOGD(TAG, "AP station disconnected event with NULL data");
        return;
    }

    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "Station " MACSTR_UM " left", MAC2STR_UM(event->mac));
}

static void ip_event_got_ip_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    if (event_data == NULL)
    {
        ESP_LOGE(TAG, "Got IP event with NULL data");
        return;
    }

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

    // Проверка, что это IP для STA (не для AP)
    if (event->esp_netif != sta_netif)
    {
        ESP_LOGD(TAG, "Got IP event for non-STA interface, ignoring");
        return;
    }

    sta_connected = true;
    ESP_LOGI(TAG, "WiFi STA Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    um_event_publish(UMNI_EVENT_WIFI_CONNECTED, NULL, 0, portMAX_DELAY);
}

static void ip_event_ap_staipassigned_handler(void *arg, esp_event_base_t event_base,
                                              int32_t event_id, void *event_data)
{
    if (event_data == NULL)
    {
        ESP_LOGD(TAG, "AP assigned IP event with NULL data");
        return;
    }

    ESP_LOGI(TAG, "AP assigned IP to station");
}

static void apply_static_ip(esp_netif_t *netif, const char *ip, const char *netmask, const char *gateway)
{
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = inet_addr(ip);
    ip_info.netmask.addr = inet_addr(netmask);
    ip_info.gw.addr = inet_addr(gateway);

    esp_netif_dhcpc_stop(netif);
    esp_netif_set_ip_info(netif, &ip_info);

    ESP_LOGI(TAG, "Static IP configured: %s", ip);
}

static void setup_sta(void)
{
    if (current_config.mode == UM_WIFI_MODE_STA || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.sta.ssid, current_config.sta.ssid);
        strcpy((char *)wifi_config.sta.password, current_config.sta.password);
        if (strlen(current_config.sta.password) == 0)
        {
            wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        }
        else
        {
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        }

        // Проверка что SSID не пустой
        if (strlen(current_config.sta.ssid) == 0)
        {
            ESP_LOGW(TAG, "STA SSID is empty, skipping STA configuration");
            return;
        }

        esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set STA config: %s", esp_err_to_name(err));
            return;
        }

        ESP_LOGI(TAG, "STA configured: %s", current_config.sta.ssid);
    }
}

static void setup_ap(void)
{
    if (current_config.mode == UM_WIFI_MODE_AP || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.ap.ssid, current_config.ap.ssid);
        strcpy((char *)wifi_config.ap.password, current_config.ap.password);
        wifi_config.ap.ssid_len = strlen(current_config.ap.ssid);
        wifi_config.ap.channel = current_config.ap.channel;
        wifi_config.ap.max_connection = current_config.ap.max_connections;

        // Изменяем authmode в зависимости от наличия пароля
        if (strlen(current_config.ap.password) == 0)
        {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }
        else
        {
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        }

        if (current_config.ap.hidden)
        {
            wifi_config.ap.ssid_hidden = 1;
        }

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_LOGI(TAG, "AP configured: %s, channel: %d, authmode: %s",
                 current_config.ap.ssid,
                 current_config.ap.channel,
                 strlen(current_config.ap.password) == 0 ? "OPEN" : "WPA2_PSK");
    }
}

static void register_event_handlers(void)
{
    // Регистрация обработчиков WiFi событий через um_events
    um_event_subscribe(WIFI_EVENT_STA_START, wifi_event_sta_start_handler, NULL);
    um_event_subscribe(WIFI_EVENT_STA_CONNECTED, wifi_event_sta_connected_handler, NULL);
    um_event_subscribe(WIFI_EVENT_STA_DISCONNECTED, wifi_event_disconnected_handler, NULL);
    um_event_subscribe(WIFI_EVENT_AP_START, wifi_event_ap_start_handler, NULL);
    um_event_subscribe(WIFI_EVENT_AP_STOP, wifi_event_ap_stop_handler, NULL);
    um_event_subscribe(WIFI_EVENT_AP_STACONNECTED, wifi_event_ap_staconnected_handler, NULL);
    um_event_subscribe(WIFI_EVENT_AP_STADISCONNECTED, wifi_event_ap_stadisconnected_handler, NULL);

    // Регистрация обработчиков IP событий ТОЛЬКО для STA режима
    if (current_config.mode == UM_WIFI_MODE_STA || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        um_event_subscribe(IP_EVENT_STA_GOT_IP, ip_event_got_ip_handler, NULL);
    }

    // Это событие для AP режима (когда клиенту выдан IP)
    if (current_config.mode == UM_WIFI_MODE_AP || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        um_event_subscribe(IP_EVENT_ASSIGNED_IP_TO_CLIENT, ip_event_ap_staipassigned_handler, NULL);
    }
}

static void unregister_event_handlers(void)
{
    um_event_unsubscribe(WIFI_EVENT_STA_START, wifi_event_sta_start_handler);
    um_event_unsubscribe(WIFI_EVENT_STA_CONNECTED, wifi_event_sta_connected_handler);
    um_event_unsubscribe(WIFI_EVENT_STA_DISCONNECTED, wifi_event_disconnected_handler);
    um_event_unsubscribe(WIFI_EVENT_AP_START, wifi_event_ap_start_handler);
    um_event_unsubscribe(WIFI_EVENT_AP_STOP, wifi_event_ap_stop_handler);
    um_event_unsubscribe(WIFI_EVENT_AP_STACONNECTED, wifi_event_ap_staconnected_handler);
    um_event_unsubscribe(WIFI_EVENT_AP_STADISCONNECTED, wifi_event_ap_stadisconnected_handler);

    if (current_config.mode == UM_WIFI_MODE_STA || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        um_event_unsubscribe(IP_EVENT_STA_GOT_IP, ip_event_got_ip_handler);
    }

    if (current_config.mode == UM_WIFI_MODE_AP || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        um_event_unsubscribe(IP_EVENT_ASSIGNED_IP_TO_CLIENT, ip_event_ap_staipassigned_handler);
    }
}

static void init_internal(um_wifi_config_t *config)
{
    if (config)
    {
        current_config = *config;
    }

    // Инициализация сетевого стека
    esp_netif_init();

    // Создание netif для STA
    if (current_config.mode == UM_WIFI_MODE_STA || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (current_config.ip_mode == UM_WIFI_IP_STATIC)
        {
            apply_static_ip(sta_netif, current_config.ip, current_config.netmask, current_config.gateway);
        }
    }

    // Создание netif для AP
    if (current_config.mode == UM_WIFI_MODE_AP || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    // Регистрация обработчиков через um_events
    register_event_handlers();

    // ВАЖНО: сначала устанавливаем режим
    ESP_ERROR_CHECK(esp_wifi_set_mode(current_config.mode));

    // ПОТОМ настраиваем интерфейсы
    setup_sta();
    setup_ap();

    ESP_ERROR_CHECK(esp_wifi_start());

    is_initialized = true;
    ESP_LOGI(TAG, "WiFi initialized in mode: %d", current_config.mode);
}

void um_wifi_init(um_wifi_config_t *config)
{
    init_internal(config);
}

void um_wifi_reinit(um_wifi_config_t *config)
{
    if (!is_initialized)
    {
        um_wifi_init(config);
        return;
    }

    ESP_LOGI(TAG, "Reinitializing WiFi...");

    esp_wifi_stop();
    esp_wifi_deinit();

    if (sta_netif)
    {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }
    if (ap_netif)
    {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }

    // Отписка от обработчиков через um_events
    unregister_event_handlers();

    init_internal(config);
}

void um_wifi_disconnect(void)
{
    if (!is_initialized)
        return;

    if (current_config.mode == UM_WIFI_MODE_STA || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        esp_wifi_disconnect();
        sta_connected = false;
        ESP_LOGI(TAG, "STA disconnected");
        um_event_publish(UMNI_EVENT_WIFI_DISCONNECTED, NULL, 0, portMAX_DELAY);
    }

    if (current_config.mode == UM_WIFI_MODE_AP || current_config.mode == UM_WIFI_MODE_APSTA)
    {
        esp_wifi_stop();
        esp_wifi_start();
        ESP_LOGI(TAG, "AP restarted");
    }
}

char *um_wifi_get_sta_ip(void)
{
    if (!sta_netif || !(current_config.mode == UM_WIFI_MODE_STA || current_config.mode == UM_WIFI_MODE_APSTA))
        return NULL;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK)
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

char *um_wifi_get_ap_ip(void)
{
    if (!ap_netif || !(current_config.mode == UM_WIFI_MODE_AP || current_config.mode == UM_WIFI_MODE_APSTA))
        return NULL;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ap_netif, &ip_info) != ESP_OK)
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

char *um_wifi_get_mac(um_wifi_mode_t iface)
{
    uint8_t mac[6];
    esp_err_t err;

    if (iface == UM_WIFI_MODE_STA)
        err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    else if (iface == UM_WIFI_MODE_AP)
        err = esp_wifi_get_mac(WIFI_IF_AP, mac);
    else
        return NULL;

    if (err != ESP_OK)
        return NULL;

    char *mac_str = malloc(18);
    if (mac_str)
    {
        sprintf(mac_str, MACSTR_UM, MAC2STR_UM(mac));
    }
    return mac_str;
}

bool um_wifi_is_sta_connected(void)
{
    return sta_connected;
}

wifi_ap_record_t *um_wifi_scan(uint16_t *count)
{
    if (!is_initialized)
        return NULL;

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(count));

    if (*count == 0)
        return NULL;

    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * (*count));
    if (records)
    {
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(count, records));
    }

    return records;
}