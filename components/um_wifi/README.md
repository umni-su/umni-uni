# UM WiFi Component

Универсальный WiFi компонент для ESP-IDF с поддержкой STA, AP и AP+STA режимов.

```c
#include "um_wifi.h"

// Базовая конфигурация STA с DHCP
um_wifi_config_t config = {
    .mode = UM_WIFI_MODE_STA,
    .ip_mode = UM_WIFI_IP_DHCP,
    .sta = {
        .ssid = "MyWiFi",
        .password = "password123",
        .timeout_s = 10
    }
};

um_wifi_init(&config);
```
## Режим STA (клиент)

```c
um_wifi_config_t config = {
    .mode = UM_WIFI_MODE_STA,
    .ip_mode = UM_WIFI_IP_DHCP,
    .sta = {
        .ssid = "HomeNetwork",
        .password = "SecurePass123",
        .timeout_s = 15
    }
};
    
    um_wifi_init(&config);

    vTaskDelay(pdMS_TO_TICKS(5000));
    
    if (um_wifi_is_sta_connected())
    {
        char *ip = um_wifi_get_sta_ip();
        ESP_LOGI("APP", "Connected! IP: %s", ip);
        free(ip);
    }
}
```

## Режим AP (точка доступа)
Создание собственной WiFi сети:

```c
void app_main(void)
{
    um_wifi_config_t config = {
        .mode = UM_WIFI_MODE_AP,
        .ip_mode = UM_WIFI_IP_STATIC,  // AP обычно использует статический IP
        .ip = "192.168.4.1",
        .netmask = "255.255.255.0",
        .gateway = "192.168.4.1",
        .ap = {
            .ssid = "MyDeviceAP",
            .password = "12345678",     // пустая строка = открытая сеть
            .channel = 6,
            .max_connections = 4,
            .hidden = false
        }
    };
    
    um_wifi_init(&config);
    
    char *ap_ip = um_wifi_get_ap_ip();
    ESP_LOGI("APP", "AP started at %s", ap_ip);
    free(ap_ip);
}
```

## Режим AP+STA
Одновременная работа в режиме клиента и точки доступа:

```c
void app_main(void)
{
    um_wifi_config_t config = {
        .mode = UM_WIFI_MODE_APSTA,
        .ip_mode = UM_WIFI_IP_DHCP,    // STA получает IP по DHCP
        .sta = {
            .ssid = "HomeNetwork",
            .password = "Pass123",
            .timeout_s = 10
        },
        .ap = {
            .ssid = "Device_AP",
            .password = "Admin123",
            .channel = 1,
            .max_connections = 3,
            .hidden = false
        }
    };
    
    um_wifi_init(&config);
    
    // Получение обоих IP адресов
    char *sta_ip = um_wifi_get_sta_ip();
    char *ap_ip = um_wifi_get_ap_ip();
    
    ESP_LOGI("APP", "STA IP: %s, AP IP: %s", sta_ip ? sta_ip : "None", ap_ip);
    
    free(sta_ip);
    free(ap_ip);
}
```

## Статический IP
Настройка статического IP для STA режима:

```c
void app_main(void)
{
    um_wifi_config_t config = {
        .mode = UM_WIFI_MODE_STA,
        .ip_mode = UM_WIFI_IP_STATIC,
        .ip = "192.168.1.100",
        .netmask = "255.255.255.0",
        .gateway = "192.168.1.1",
        .dns = "8.8.8.8",
        .sta = {
            .ssid = "OfficeWiFi",
            .password = "CorpPass2024",
            .timeout_s = 10
        }
    };
    
    um_wifi_init(&config);
}
```

## Переинициализация
Смена конфигурации без перезагрузки устройства:

```c
void change_wifi_config(void)
{
    // Отключаем текущее подключение
    um_wifi_disconnect();
    
    // Новая конфигурация
    um_wifi_config_t new_config = {
        .mode = UM_WIFI_MODE_STA,
        .ip_mode = UM_WIFI_IP_DHCP,
        .sta = {
            .ssid = "NewNetwork",
            .password = "NewPassword",
            .timeout_s = 10
        }
    };
    
    // Переинициализация с новыми параметрами
    um_wifi_reinit(&new_config);
}

void switch_to_ap_mode(void)
{
    um_wifi_config_t ap_config = {
        .mode = UM_WIFI_MODE_AP,
        .ip_mode = UM_WIFI_IP_STATIC,
        .ip = "192.168.4.1",
        .netmask = "255.255.255.0",
        .gateway = "192.168.4.1",
        .ap = {
            .ssid = "EmergencyAP",
            .password = "TempPass",
            .channel = 11,
            .max_connections = 1
        }
    };
    
    um_wifi_reinit(&ap_config);
}
```

## Отключение
Отключение WiFi без переинициализации:

```c
void handle_disconnect(void)
{
    // Отключение STA и перезапуск AP при необходимости
    um_wifi_disconnect();
    
    ESP_LOGI("APP", "WiFi disconnected");
    
    // Проверка статуса
    if (!um_wifi_is_sta_connected())
    {
        ESP_LOGI("APP", "STA is disconnected");
    }
}
```

## Сканирование сетей
Поиск доступных WiFi сетей:

```c
void scan_networks(void)
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *aps = um_wifi_scan(&ap_count);
    
    if (aps && ap_count > 0)
    {
        ESP_LOGI("SCAN", "Found %d networks:", ap_count);
        
        for (int i = 0; i < ap_count; i++)
        {
            ESP_LOGI("SCAN", "  %d. %s | RSSI: %d | Channel: %d | Auth: %d",
                     i + 1,
                     aps[i].ssid,
                     aps[i].rssi,
                     aps[i].primary,
                     aps[i].authmode);
        }
        
        free(aps);
    }
    else
    {
        ESP_LOGW("SCAN", "No networks found");
    }
}
```

## Получение MAC адреса

```c
void print_mac_addresses(void)
{
    char *sta_mac = um_wifi_get_mac(UM_WIFI_MODE_STA);
    char *ap_mac = um_wifi_get_mac(UM_WIFI_MODE_AP);
    
    if (sta_mac)
    {
        ESP_LOGI("MAC", "STA MAC: %s", sta_mac);
        free(sta_mac);
    }
    
    if (ap_mac)
    {
        ESP_LOGI("MAC", "AP MAC: %s", ap_mac);
        free(ap_mac);
    }
}
```

## Комплексный пример с обработкой событий

```c
#include "um_wifi.h"
#include "um_events.h"

// Обработчик событий (если используется um_events)
static void wifi_event_callback(void *arg)
{
    // Обработка событий подключения/отключения
}

void app_main(void)
{
    // Подписка на события (опционально)
    um_event_subscribe(UMNI_EVENT_WIFI_CONNECTED, wifi_event_callback, NULL);
    um_event_subscribe(UMNI_EVENT_WIFI_DISCONNECTED, wifi_event_callback, NULL);
    
    // Конфигурация с автоматическим переподключением
    um_wifi_config_t config = {
        .mode = UM_WIFI_MODE_APSTA,
        .ip_mode = UM_WIFI_IP_DHCP,
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .timeout_s = 30
        },
        .ap = {
            .ssid = CONFIG_ESP_WIFI_AP_SSID,
            .password = CONFIG_ESP_WIFI_AP_PASSWORD,
            .channel = CONFIG_ESP_WIFI_CHANNEL,
            .max_connections = CONFIG_ESP_MAX_STA_CONN,
            .hidden = false
        }
    };
    
    um_wifi_init(&config);
    
    // Основной цикл приложения
    while (1)
    {
        if (um_wifi_is_sta_connected())
        {
            char *ip = um_wifi_get_sta_ip();
            ESP_LOGI("MAIN", "WiFi connected. IP: %s", ip);
            free(ip);
            
            // Выполнение HTTP запросов и т.д.
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
```