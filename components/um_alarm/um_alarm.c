/**
 * @file um_alarm.c
 * @brief Alarm input implementation with interrupt
 */

#include "um_alarm.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char* TAG = "um_alarm";

#if defined(CONFIG_UM_FEATURE_ALARM)

// Static context
static struct {
    bool initialized;
    int gpio_num;
    um_alarm_edge_t edge;
    TaskHandle_t task_handle;
    QueueHandle_t event_queue;
    um_alarm_callback_t user_callback;
    void* user_data;
    volatile uint32_t trigger_count;
    int64_t last_isr_time;      // Время последнего прерывания
    bool last_state;           // Последнее стабильное состояние
    int debounce_time_ms;      // Время подавления дребезга (мс)
} alarm_ctx = {
    .initialized = false,
    .gpio_num = CONFIG_UM_CFG_ALARM_GPIO,
    .edge = UM_ALARM_EDGE_FALLING,
    .task_handle = NULL,
    .event_queue = NULL,
    .user_callback = NULL,
    .user_data = NULL,
    .trigger_count = 0,
    .last_isr_time = 0,
    .last_state = false,
    .debounce_time_ms = 50     // 50ms по умолчанию
};

// Event structure for task queue
typedef struct {
    bool state;
    uint32_t count;
} alarm_event_t;

// Interrupt handler (must be in IRAM)
static void IRAM_ATTR alarm_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    alarm_event_t event;
    
    // Время текущего прерывания
    int64_t now = esp_timer_get_time() / 1000; // конвертируем в мс
    
    // Чтение текущего состояния
    bool current_state = gpio_get_level(alarm_ctx.gpio_num);
    
    // Подавление дребезга по времени
    if ((now - alarm_ctx.last_isr_time) < alarm_ctx.debounce_time_ms) {
        // Слишком быстро после предыдущего прерывания - игнорируем
        return;
    }
    
    // Определяем тип фронта
    bool edge_detected = false;
    
    switch (alarm_ctx.edge) {
        case UM_ALARM_EDGE_FALLING:
            if (alarm_ctx.last_state && !current_state) {
                edge_detected = true;  // FALLING
            }
            break;
            
        case UM_ALARM_EDGE_RISING:
            if (!alarm_ctx.last_state && current_state) {
                edge_detected = true;  // RISING
            }
            break;
            
        case UM_ALARM_EDGE_BOTH:
            if (alarm_ctx.last_state != current_state) {
                edge_detected = true;  // Любое изменение
            }
            break;
    }
    
    // Если фронт обнаружен
    if (edge_detected) {
        // Обновляем время и состояние
        alarm_ctx.last_isr_time = now;
        alarm_ctx.last_state = current_state;
        
        // Увеличиваем счетчик
        alarm_ctx.trigger_count++;
        
        // Готовим событие
        event.state = current_state;
        event.count = alarm_ctx.trigger_count;
        
        // Отправляем в очередь
        if (alarm_ctx.event_queue) {
            xQueueSendFromISR(alarm_ctx.event_queue, &event, &xHigherPriorityTaskWoken);
        }
        
        // Yield если нужно
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}
// Task to handle events from ISR
static void alarm_task(void* arg) {
    alarm_event_t event;
    
    while (1) {
        // Wait for event from ISR
        if (xQueueReceive(alarm_ctx.event_queue, &event, portMAX_DELAY)) {
            // Call user callback if set
            if (alarm_ctx.user_callback) {
                alarm_ctx.user_callback(event.state, alarm_ctx.user_data);
            }
            
            ESP_LOGI(TAG, "Alarm trigger #%u, state: %s", 
                     event.count, event.state ? "HIGH" : "LOW");
        }
    }
}

esp_err_t um_alarm_init(um_alarm_edge_t edge, bool pull_up, bool pull_down, int debounce_ms) {
    if (alarm_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    // Validate GPIO
    if (alarm_ctx.gpio_num < 0 || alarm_ctx.gpio_num > 34) {
        ESP_LOGE(TAG, "Invalid GPIO: %d", alarm_ctx.gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << alarm_ctx.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE, // Set later based on edge
    };
    
    // Set interrupt type based on edge
    switch (edge) {
        case UM_ALARM_EDGE_FALLING:
            io_conf.intr_type = GPIO_INTR_NEGEDGE;
            break;
        case UM_ALARM_EDGE_RISING:
            io_conf.intr_type = GPIO_INTR_POSEDGE;
            break;
        case UM_ALARM_EDGE_BOTH:
            io_conf.intr_type = GPIO_INTR_ANYEDGE;
            break;
        default:
            io_conf.intr_type = GPIO_INTR_NEGEDGE;
            break;
    }
    
    alarm_ctx.edge = edge;
    alarm_ctx.last_state = gpio_get_level(alarm_ctx.gpio_num);
    alarm_ctx.debounce_time_ms = (debounce_ms > 0) ? debounce_ms : 50;
    alarm_ctx.last_isr_time = esp_timer_get_time() / 1000;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create event queue
    alarm_ctx.event_queue = xQueueCreate(10, sizeof(alarm_event_t));
    if (!alarm_ctx.event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Create task for handling events
    BaseType_t task_ret = xTaskCreate(alarm_task, "alarm_task", 2048, NULL, 5, &alarm_ctx.task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        vQueueDelete(alarm_ctx.event_queue);
        alarm_ctx.event_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    // Install ISR handler
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        vTaskDelete(alarm_ctx.task_handle);
        vQueueDelete(alarm_ctx.event_queue);
        alarm_ctx.task_handle = NULL;
        alarm_ctx.event_queue = NULL;
        return ret;
    }
    
    // Add ISR handler for GPIO
    ret = gpio_isr_handler_add(alarm_ctx.gpio_num, alarm_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        gpio_uninstall_isr_service();
        vTaskDelete(alarm_ctx.task_handle);
        vQueueDelete(alarm_ctx.event_queue);
        alarm_ctx.task_handle = NULL;
        alarm_ctx.event_queue = NULL;
        return ret;
    }
    
    alarm_ctx.initialized = true;
    alarm_ctx.trigger_count = 0;
    
    ESP_LOGI(TAG, "Alarm initialized on GPIO %d, edge: %d, pull: %s/%s", 
             alarm_ctx.gpio_num, edge,
             pull_up ? "UP" : "no",
             pull_down ? "DOWN" : "no");
    
    // Log initial state
    bool initial_state;
    um_alarm_get_state(&initial_state);
    ESP_LOGI(TAG, "Initial state: %s", initial_state ? "HIGH" : "LOW");
    
    return ESP_OK;
}

esp_err_t um_alarm_set_debounce(int debounce_ms) {
    if (!alarm_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (debounce_ms < 0 || debounce_ms > 1000) {
        return ESP_ERR_INVALID_ARG;
    }
    
    alarm_ctx.debounce_time_ms = debounce_ms;
    ESP_LOGI(TAG, "Debounce time set to %d ms", debounce_ms);
    
    return ESP_OK;
}

esp_err_t um_alarm_set_callback(um_alarm_callback_t callback, void* user_data) {
    if (!alarm_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    alarm_ctx.user_callback = callback;
    alarm_ctx.user_data = user_data;
    
    ESP_LOGI(TAG, "Callback %s", callback ? "set" : "cleared");
    return ESP_OK;
}

esp_err_t um_alarm_get_state(bool* state) {
    if (!alarm_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *state = gpio_get_level(alarm_ctx.gpio_num);
    return ESP_OK;
}

esp_err_t um_alarm_get_count(uint32_t* count) {
    if (!alarm_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *count = alarm_ctx.trigger_count;
    return ESP_OK;
}

esp_err_t um_alarm_reset_count(void) {
    if (!alarm_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    alarm_ctx.trigger_count = 0;
    ESP_LOGI(TAG, "Trigger count reset");
    return ESP_OK;
}

esp_err_t um_alarm_deinit(void) {
    if (!alarm_ctx.initialized) {
        return ESP_OK;
    }
    
    // Disable interrupt
    gpio_isr_handler_remove(alarm_ctx.gpio_num);
    
    // Delete task
    if (alarm_ctx.task_handle) {
        vTaskDelete(alarm_ctx.task_handle);
        alarm_ctx.task_handle = NULL;
    }
    
    // Delete queue
    if (alarm_ctx.event_queue) {
        vQueueDelete(alarm_ctx.event_queue);
        alarm_ctx.event_queue = NULL;
    }
    
    // Reset GPIO
    gpio_reset_pin(alarm_ctx.gpio_num);
    
    // Clear callback
    alarm_ctx.user_callback = NULL;
    alarm_ctx.user_data = NULL;
    
    alarm_ctx.initialized = false;
    ESP_LOGI(TAG, "Alarm deinitialized");
    
    return ESP_OK;
}

#endif // CONFIG_UM_FEATURE_ALARM