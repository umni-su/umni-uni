#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"

// Структура для информации о сетевом интерфейсе
typedef struct
{
    char interface_name[16]; // "eth", "wifi_sta", "wifi_ap"
    char ip_address[16];     // XXX.XXX.XXX.XXX
    char netmask[16];        // XXX.XXX.XXX.XXX
    char gateway[16];        // XXX.XXX.XXX.XXX
    char mac_address[18];    // "xx:xx:xx:xx:xx:xx" + null terminator
    uint8_t is_active;       // 1 если интерфейс активен
} um_network_interface_info_t;

// Структура для информации о памяти
typedef struct
{
    uint32_t total_heap;    // Всего heap памяти
    uint32_t free_heap;     // Свободной heap памяти
    uint32_t min_free_heap; // Минимально свободной heap памяти
} um_memory_info_t;

/**
 * @brief Получает информацию о всех сетевых интерфейсах
 * @param interfaces Массив для заполнения структур интерфейсов
 * @param max_count Максимальное количество интерфейсов
 * @return int Количество найденных интерфейсов
 */
int um_helpers_get_network_interfaces(um_network_interface_info_t *interfaces, int max_count);

/**
 * @brief Получает информацию о доступной памяти
 * @param info Указатель на структуру для заполнения
 * @return int 0 при успехе, -1 при ошибке
 */
int um_helpers_get_memory_info(um_memory_info_t *info);

char *um_helpers_generate_device_name_from_mac(const char *prefix, char *buffer, size_t buffer_size);
char *um_helpers_generate_device_name_full_mac(const char *prefix, char *buffer, size_t buffer_size);
esp_err_t um_helpers_get_systeminfo(cJSON **data);

void um_helpers_time_init(void);
uint64_t um_helpers_get_real_timestamp_ms(void);

typedef struct
{
    const char *key;
    const char *value;
} um_mdns_txt_item_t;

// Инициализация mDNS сервиса
esp_err_t um_helpers_mdns_init(void);

// Добавление базовых сервисов для обнаружения
esp_err_t um_mdns_add_basic_services(void);

esp_err_t um_helpers_get_hostname(char **hostname);

// Добавление кастомного сервиса
esp_err_t um_mdns_add_service(const char *instance_name,
                              const char *service_type,
                              const char *proto,
                              uint16_t port,
                              um_mdns_txt_item_t *txt_items,
                              size_t txt_count);

esp_err_t um_mdns_add_discovery(void);

bool um_helpers_mdns_running(void);
