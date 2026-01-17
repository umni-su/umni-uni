# ============================================
# UMNI C1 Configuration
# ============================================

# Device Info
set(UMNI_DEVICE_NAME "UMNI C1")
set(UMNI_DEVICE_MODEL "CTRL-001")

# Firmware Version
set(UMNI_FW_VERSION_MAJOR 1)
set(UMNI_FW_VERSION_MINOR 0)
set(UMNI_FW_VERSION_PATCH 0)
set(UMNI_FW_VERSION "${UMNI_FW_VERSION_MAJOR}.${UMNI_FW_VERSION_MINOR}.${UMNI_FW_VERSION_PATCH}")

# ============================================
# FEATURES (все включены для C1)
# ============================================

# Network Interfaces
set(UM_FEATURE_ETH ON)           # Ethernet
set(UM_FEATURE_SD ON)            # MicroSD
set(UM_FEATURE_WIFI ON)          # WiFi

# Communication Protocols
set(UM_FEATURE_HTTP_SRV ON)      # HTTP REST Server
set(UM_FEATURE_HTTP_WH ON)       # HTTP Webhooks
set(UM_FEATURE_MQTT ON)          # MQTT Client
set(UM_FEATURE_OT ON)            # OpenTherm
set(UM_FEATURE_RF433 ON)         # RF433
set(UM_FEATURE_OW ON)            # 1-Wire

# Sensors and Inputs
set(UM_FEATURE_ALARM_IN ON)      # Alarm 12V Input
set(UM_FEATURE_NTC1 ON)          # NTC Sensor 1
set(UM_FEATURE_NTC2 ON)          # NTC Sensor 2
set(UM_FEATURE_AI1 ON)           # Analog Input 1
set(UM_FEATURE_AI2 ON)           # Analog Input 2

# Outputs
set(UM_FEATURE_OC1 ON)           # Open Collector 1
set(UM_FEATURE_OC2 ON)           # Open Collector 2
set(UM_FEATURE_BUZZER ON)        # Buzzer

# Digital Inputs (PCF8574)
set(UM_FEATURE_INP1 ON)          # Input 1
set(UM_FEATURE_INP2 ON)          # Input 2
set(UM_FEATURE_INP3 ON)          # Input 3
set(UM_FEATURE_INP4 ON)          # Input 4
set(UM_FEATURE_INP5 ON)          # Input 5
set(UM_FEATURE_INP6 ON)          # Input 6

# Digital Outputs (PCF8574)
set(UM_FEATURE_OUT1 ON)          # Output 1
set(UM_FEATURE_OUT2 ON)          # Output 2
set(UM_FEATURE_OUT3 ON)          # Output 3
set(UM_FEATURE_OUT4 ON)          # Output 4
set(UM_FEATURE_OUT5 ON)          # Output 5
set(UM_FEATURE_OUT6 ON)          # Output 6
set(UM_FEATURE_OUT7 ON)          # Output 7
set(UM_FEATURE_OUT8 ON)          # Output 8

# ============================================
# PIN DEFINITIONS
# ============================================

# Ethernet (W5500 LITE)
set(UM_CFG_MOSI 21)     # SPI MOSI
set(UM_CFG_MISO 19)     # SPI MISO
set(UM_CFG_CLK 18)      # SPI CLK
set(UM_CFG_ETH_CS 5)    # Ethernet CS
set(UM_CFG_ETH_INT 4)   # Ethernet INT

# MicroSD (TFP09-2-12B)
set(UM_CFG_SD_CS 32)    # MicroSD CS
set(UM_CFG_SD_CD 33)    # MicroSD Card Detect

# OpenTherm
set(UM_CFG_OT_IN 26)    # OpenTherm Input
set(UM_CFG_OT_OUT 25)   # OpenTherm Output

# RF433
set(UM_CFG_RF433_DATA 27)  # RF433 Data Pin

# 1-Wire
set(UM_CFG_ONEWIRE_PIN 17)  # 1-Wire Bus

# Alarm Input
set(UM_CFG_ALARM_PIN 13)    # Alarm 12V Input

# NTC Sensors
set(UM_CFG_NTC1_PIN 36)     # NTC Sensor 1
set(UM_CFG_NTC2_PIN 39)     # NTC Sensor 2

# Analog Inputs
set(UM_CFG_AI1_PIN 34)      # Analog Input 1
set(UM_CFG_AI2_PIN 35)      # Analog Input 2

# Open Collector Outputs
set(UM_CFG_OC1_PIN 14)      # Open Collector 1
set(UM_CFG_OC2_PIN 12)      # Open Collector 2

# Buzzer
set(UM_CFG_BUZZER 15)       # Buzzer

# I2C for PCF8574
set(UM_CFG_SDA 23)          # I2C SDA
set(UM_CFG_SCL 22)          # I2C SCL

# PCF8574 Input Indices (первые 6 каналов)
set(UM_CFG_INP1_INDEX 1)    # Input 1 Index
set(UM_CFG_INP2_INDEX 2)    # Input 2 Index
set(UM_CFG_INP3_INDEX 3)    # Input 3 Index
set(UM_CFG_INP4_INDEX 4)    # Input 4 Index
set(UM_CFG_INP5_INDEX 5)    # Input 5 Index
set(UM_CFG_INP6_INDEX 6)    # Input 6 Index

# PCF8574 Output Indices (вторые 8 каналов)
set(UM_CFG_OUT1_INDEX 1)    # Output 1 Index
set(UM_CFG_OUT2_INDEX 2)    # Output 2 Index
set(UM_CFG_OUT3_INDEX 3)    # Output 3 Index
set(UM_CFG_OUT4_INDEX 4)    # Output 4 Index
set(UM_CFG_OUT5_INDEX 5)    # Output 5 Index
set(UM_CFG_OUT6_INDEX 6)    # Output 6 Index
set(UM_CFG_OUT7_INDEX 7)    # Output 7 Index
set(UM_CFG_OUT8_INDEX 8)    # Output 8 Index