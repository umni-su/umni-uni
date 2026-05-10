idf_component_register(
    SRCS "um_sse_server.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_netif esp_http_server"  
)