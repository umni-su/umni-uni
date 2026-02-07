idf_component_register(
    SRCS "um_onewire.c" "um_onewire_config.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_event json"  
)