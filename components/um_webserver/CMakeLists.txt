idf_component_register(
    SRCS "um_webserver.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_netif esp_http_server json"  
)