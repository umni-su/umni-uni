cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# ============================================
# 1. ВЫБОР УСТРОЙСТВА
# ============================================
set(UMNI_DEVICE "umni-c1" CACHE STRING "Choose device")
set_property(CACHE UMNI_DEVICE PROPERTY STRINGS umni-c1 umni-ot umni-skud)

message(STATUS "========================================")
message(STATUS "BUILD FOR: ${UMNI_DEVICE}")
message(STATUS "========================================")

# ============================================
# 2. ЗАГРУЗКА КОНФИГА
# ============================================
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/configs/${UMNI_DEVICE}.cmake")

if(EXISTS ${CONFIG_FILE})
    message(STATUS "Use config file: ${CONFIG_FILE}")
    include(${CONFIG_FILE})
else()
    message(FATAL_ERROR "Config file not found: ${CONFIG_FILE}")
endif()

# ============================================
# 3. ПЕРЕДАЧА ПАРАМЕТРОВ В КОД
# ============================================
# Устанавливаем глобальные define
add_compile_definitions(
    UMNI_DEVICE_NAME="${UMNI_DEVICE_NAME}"
    UMNI_DEVICE_MODEL="${UMNI_DEVICE_MODEL}"
    UMNI_DEVICE_TYPE="${UMNI_DEVICE}"
)

# Версия прошивки
if(DEFINED UMNI_FW_VERSION)
    add_compile_definitions(UMNI_FW_VERSION="${UMNI_FW_VERSION}")
    message(STATUS "Firmware version: ${UMNI_FW_VERSION}")
else()
    add_compile_definitions(UMNI_FW_VERSION="0.0.0")
    message(STATUS "Firmware version: 0.0.0 (default)")
endif()

# ============================================
# ВСЕ ФИЧИ ДЛЯ UMNI C1
# ============================================
set(ALL_FEATURES
    ETH SD WIFI HTTP_SRV HTTP_WH MQTT OT RF433 OW 
    ALARM_IN NTC1 NTC2 AI1 AI2 OC1 OC2 BUZZER
    INP1 INP2 INP3 INP4 INP5 INP6
    OUT1 OUT2 OUT3 OUT4 OUT5 OUT6 OUT7 OUT8
)

foreach(FEATURE ${ALL_FEATURES})
    if(DEFINED UM_FEATURE_${FEATURE})
        if(${UM_FEATURE_${FEATURE}})
            add_compile_definitions(UM_FEATURE_${FEATURE}=1)
            message(STATUS "✓ ${FEATURE}: ON")
        else()
            add_compile_definitions(UM_FEATURE_${FEATURE}=0)
            message(STATUS "✗ ${FEATURE}: OFF")
        endif()
    else()
        # Если фича не определена в конфиге - считаем выключенной
        add_compile_definitions(UM_FEATURE_${FEATURE}=0)
        message(STATUS "✗ ${FEATURE}: OFF (not defined)")
    endif()
endforeach()

# ============================================
# ВСЕ ПИНЫ ДЛЯ UMNI C1
# ============================================
set(ALL_PINS
    # SPI/Ethernet
    MOSI MISO CLK CS INT
    # MicroSD
    SD_CS SD_CD
    # OpenTherm
    OT_IN OT_OUT
    # RF433
    RF433_DATA
    # 1-Wire
    ONEWIRE_PIN
    # Alarm
    ALARM_PIN
    # NTC Sensors
    NTC1_PIN NTC2_PIN
    # Analog Inputs
    AI1_PIN AI2_PIN
    # Open Collector
    OC1_PIN OC2_PIN
    # Buzzer
    BUZZER
    # I2C
    SDA SCL
    # PCF8574 Input Indices
    INP1_INDEX INP2_INDEX INP3_INDEX INP4_INDEX INP5_INDEX INP6_INDEX
    # PCF8574 Output Indices
    OUT1_INDEX OUT2_INDEX OUT3_INDEX OUT4_INDEX OUT5_INDEX 
    OUT6_INDEX OUT7_INDEX OUT8_INDEX
)

foreach(PIN ${ALL_PINS})
    if(DEFINED UM_CFG_${PIN})
        add_compile_definitions(UM_CFG_${PIN}=${UM_CFG_${PIN}})
        message(STATUS "  ${PIN}: ${UM_CFG_${PIN}}")
    else()
        # Если пин не определен - ставим 0
        add_compile_definitions(UM_CFG_${PIN}=0)
        message(STATUS "  ${PIN}: 0 (not defined)")
    endif()
endforeach()

message(STATUS "========================================")

# ============================================
# 4. ЗАПУСК ПРОЕКТА
# ============================================
project(umni-uni)