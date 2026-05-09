```c
// DHCP
um_ethernet_init(NULL);

// Статический IP
um_eth_config_t config = {
    .mode = UM_ETH_MODE_STATIC,
    .ip = "192.168.1.100",
    .netmask = "255.255.255.0",
    .gateway = "192.168.1.1",
    .dns = "8.8.8.8"
};
um_ethernet_init(&config);

// Переинициализация
config.mode = UM_ETH_MODE_DHCP;
um_ethernet_reinit(&config);

// Получить текущий IP
char *ip = um_ethernet_get_ip();
if (ip) {
    ESP_LOGI(TAG, "Current IP: %s", ip);
    free(ip);
}
```