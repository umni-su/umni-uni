#include "um_sd.h"
#include "base_config.h"
#include "um_events.h"

// Время подавления дребезга контактов в миллисекундах
#define DEBOUNCE_DELAY_MS 50

#if UM_FEATURE_ENABLED(SDCARD)

static const char *TAG = "sdcard";
static TickType_t last_interrupt_time = 0;
static sdmmc_card_t *sd_card = NULL;

/**
 * @brief Задача обработки прерывания детектора SD карты
 */
static void um_sd_cd_interrupt_task(void *arg)
{
    int level = gpio_get_level(CONFIG_UM_CFG_SDCARD_DETECT_GPIO);
    printf("Level %d ", level);
    if (level == 0)
    {
        esp_event_post(UMNI_EVENT_BASE, UMNI_EVENT_SDCARD_PUSH_IN, NULL, sizeof(NULL), portMAX_DELAY);
        ESP_LOGI(TAG, "SD card was inserted %d", level);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        um_sd_mount();
    }
    else
    {
        esp_event_post(UMNI_EVENT_BASE, UMNI_EVENT_SDCARD_PUSH_OUT, NULL, sizeof(NULL), portMAX_DELAY);
        ESP_LOGW(TAG, "SD card was ejected %d", level);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        um_sd_unmount();
    }

    esp_restart();

    vTaskDelete(NULL);
}

/**
 * @brief Обработчик прерывания GPIO в режиме IRAM
 */
static void IRAM_ATTR um_catch_sd_cd_interrupts(void *args)
{
    TickType_t current_time = xTaskGetTickCountFromISR();

    // Подавление дребезга - проверяем время с последнего прерывания
    if ((current_time - last_interrupt_time) * portTICK_PERIOD_MS >= DEBOUNCE_DELAY_MS)
    {
        xTaskCreate(um_sd_cd_interrupt_task, "sd_cd_interrupt_task", 4096, NULL, 2, NULL);
    }

    last_interrupt_time = current_time;
}

/**
 * @brief Инициализация детектора SD карты
 */
void um_init_sd_cd(void)
{

    // Убедитесь, что служба прерываний GPIO установлена
    esp_err_t res = gpio_install_isr_service(0);

    if(res == ESP_ERR_INVALID_STATE){
        ESP_LOGI(TAG, "SD CD interrupt handler already installed");
    }

    gpio_reset_pin(CONFIG_UM_CFG_SDCARD_DETECT_GPIO);
    gpio_set_direction(CONFIG_UM_CFG_SDCARD_DETECT_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(CONFIG_UM_CFG_SDCARD_DETECT_GPIO, GPIO_FLOATING);
    gpio_isr_handler_add(CONFIG_UM_CFG_SDCARD_DETECT_GPIO, um_catch_sd_cd_interrupts, NULL);
    gpio_set_intr_type(CONFIG_UM_CFG_SDCARD_DETECT_GPIO, GPIO_INTR_ANYEDGE);
    gpio_intr_enable(CONFIG_UM_CFG_SDCARD_DETECT_GPIO);

    int level = gpio_get_level(CONFIG_UM_CFG_SDCARD_DETECT_GPIO);

    ESP_LOGI(TAG, "SD CD interrupt handler initialized with debouncing (level %d)", level);
}

/**
 * @brief Проверяет текущее состояние детектора SD карты
 */
bool um_sd_card_detected(void)
{
    int level = gpio_get_level(CONFIG_UM_CFG_SDCARD_DETECT_GPIO);
    return (level == 0);
}

/**
 * @brief Инициализирует начальное сотояние SD карты
 */
esp_err_t um_sd_init()
{
    // Инициализация детектора
    um_init_sd_cd();

    // Проверка наличия карты
    if (um_sd_card_detected())
    {
        // Монтирование карты
        return um_sd_mount();
    }
    return ESP_FAIL;
}
/**
 * @brief Монтирует SD карту в файловую систему
 */
esp_err_t um_sd_mount(void)
{
    esp_err_t ret;

    // Конфигурация монтирования FAT
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // Конфигурация устройства SD SPI
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = CONFIG_UM_CFG_SDCARD_SPI_HOST;
    slot_config.gpio_cs = CONFIG_UM_CFG_SDCARD_CS_GPIO;

    // Настройка хоста SD SPI
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 12 * 1000; // Пониженная частота для стабильности

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = CONFIG_UM_CFG_SDCARD_MOSI_GPIO,
        .miso_io_num = CONFIG_UM_CFG_SDCARD_MISO_GPIO,
        .sclk_io_num = CONFIG_UM_CFG_SDCARD_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return ESP_FAIL;
    }

    // Монтирование SD карты
    ret = esp_vfs_fat_sdspi_mount(CONFIG_UMNI_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card mounted successfully");
        // Вывод информации о карте
        //sdmmc_card_print_info(stdout, sd_card);
        char *type;
        if (sd_card->is_sdio) {
            type = "SDIO";
        } else if (sd_card->is_mmc) {
            type = "MMC";
        } else {
                if ((sd_card->ocr & (1<<30)) == 0) {
                    type = "SDSC";
                } else {
                    if (sd_card->ocr & (1<<24)) {
                        type = "SDHC/SDXC (UHS-I)";
                    } else {
                        type = "SDHC";
                    }
                }
        }
        uint64_t size = ((uint64_t) sd_card->csd.capacity) * sd_card->csd.sector_size / (1024 * 1024);
        ESP_LOGI(TAG, "✅ SD Card name: %s, type: %s, capacity: %llu MB",sd_card->cid.name, type, size);
        esp_event_post(UMNI_EVENT_BASE, UMNI_EVENT_SDCARD_MOUNTED, NULL, sizeof(NULL), portMAX_DELAY);
    }
    else
    {
        ESP_LOGE(TAG, "❌ Failed to mount SD card: %s", esp_err_to_name(ret));
        esp_event_post(UMNI_EVENT_BASE, UMNI_EVENT_SDCARD_UNMOUNTED, NULL, sizeof(NULL), portMAX_DELAY);
    }

    return ret;
}

/**
 * @brief Размонтирует SD карту
 */
esp_err_t um_sd_unmount(void)
{
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(CONFIG_UMNI_SD_MOUNT_POINT, sd_card);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card unmounted successfully");
        sd_card = NULL;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief Получает информацию о SD карте
 */
void *um_sd_get_card_info(void)
{
    return sd_card;
}

#endif