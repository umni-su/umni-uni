idf_component_register(
    SRCS "um_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos
        esp_netif
        esp_wifi
        esp_event
        nvs_flash
        lwip
)