```c
// Инициализация
um_nvs_init();
um_nvs_open("storage");

// Чтение значения
char* ssid = um_nvs_get_wifi_sta_ssid();
if (ssid != NULL) {
    // Использование ssid
    free(ssid); // Не забыть освободить!
}

// Запись значения
um_nvs_set_wifi_sta_ssid("MyWiFi");

// Закрытие
um_nvs_close();
```