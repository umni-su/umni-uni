cmake_minimum_required(VERSION 3.16)

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
set(CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/configs/sdkconfig.defaults.${UMNI_DEVICE}")

file(GLOB COMPONENT_DIRS RELATIVE "${COMPONENTS_BASE_DIR}" "${COMPONENTS_BASE_DIR}/um_*")

foreach(COMPONENT_DIR ${COMPONENT_DIRS})
    get_filename_component(COMPONENT_NAME ${COMPONENT_DIR} NAME)
    string(REPLACE "um_" "" COMPONENT ${COMPONENT_NAME})
    string(TOUPPER ${COMPONENT} COMPONENT_UPPER)
    
    if(${UM_FEATURE_${COMPONENT_UPPER}})
        # Используем абсолютный путь для добавления в список EXTRA_COMPONENT_DIRS
        list(APPEND EXTRA_COMPONENT_DIRS "${COMPONENTS_BASE_DIR}/${COMPONENT_DIR}")
        message(STATUS "✓ Component: ${COMPONENT_NAME} added to EXTRA_COMPONENT_DIRS")
    else()
        message(STATUS "✗ Component: ${COMPONENT_NAME} (feature disabled)")
    endif()
endforeach()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/configs)

# ============================================
# 4. ЗАПУСК ПРОЕКТА
# ============================================

set(SDKCONFIG_DEFAULTS 
    "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults.esp32"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults.esp32"
    "${CONFIG_FILE}"
    CACHE INTERNAL "Base SDK config defaults"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD OFF)

project(umni-uni)