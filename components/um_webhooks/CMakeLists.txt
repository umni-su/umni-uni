idf_component_register(
    SRCS "um_webhooks.c"
    INCLUDE_DIRS "include"
    REQUIRES "json" "esp_http_client"  
)