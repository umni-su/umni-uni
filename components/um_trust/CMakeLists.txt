idf_component_register(
    SRCS "um_trust.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_netif esp_http_server esp_http_client mbedtls"
    PRIV_REQUIRES "um_nvs um_storage um_capabilities um_helpers um_sockets"
)