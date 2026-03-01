idf_component_register(
    SRCS "um_adc_config.c" "um_adc.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_adc esp_event json"  
)