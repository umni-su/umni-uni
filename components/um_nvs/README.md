```c
// Пример 1: Безопасное чтение конфигурации
void read_device_config(void) {
    char* hostname = NULL;
    uint8_t network_mode = 0;
    bool ot_enabled = false;
    
    esp_err_t err1 = um_nvs_get_hostname(&hostname);
    esp_err_t err2 = um_nvs_get_network_mode(&network_mode);
    esp_err_t err3 = um_nvs_get_ot_enabled(&ot_enabled);
    
    if (err1 == ESP_OK && hostname != NULL) {
        ESP_LOGI("APP", "Hostname: %s", hostname);
        free(hostname);
    }
    
    if (err2 == ESP_OK) {
        ESP_LOGI("APP", "Network mode: %u", network_mode);
    }
    
    if (err3 == ESP_OK) {
        ESP_LOGI("APP", "OpenTherm: %s", ot_enabled ? "Enabled" : "Disabled");
    }
}

// Пример 2: Чтение с обработкой ошибок
esp_err_t configure_wifi_connection(void) {
    char* ssid = NULL;
    char* password = NULL;
    
    esp_err_t err = um_nvs_get_wifi_sta_ssid(&ssid);
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to read WiFi SSID: %s", esp_err_to_name(err));
        return err;
    }
    
    err = um_nvs_get_wifi_sta_password(&password);
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to read WiFi password: %s", esp_err_to_name(err));
        free(ssid);
        return err;
    }
    
    if (ssid == NULL || password == NULL) {
        ESP_LOGE("APP", "WiFi credentials incomplete");
        free(ssid);
        free(password);
        return ESP_FAIL;
    }
    
    // Используем credentials для подключения к WiFi...
    
    free(ssid);
    free(password);
    return ESP_OK;
}

// Пример 3: Запись с проверкой
esp_err_t update_thermostat_settings(void) {
    esp_err_t err = um_nvs_set_ot_ch_setpoint(22);
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to update CH setpoint: %s", esp_err_to_name(err));
        return err;
    }
    
    err = um_nvs_set_ot_dhw_setpoint(55);
    if (err != ESP_OK) {
        ESP_LOGE("APP", "Failed to update DHW setpoint: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI("APP", "Thermostat settings updated");
    return ESP_OK;
}
```