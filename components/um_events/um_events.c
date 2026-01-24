/**
 * @file um_events.c
 * @brief Implementation of simple event bus for ESP-IDF
 */

#include "um_events.h"
#include "esp_log.h"

static const char* TAG = "um_events";

/**
 * @brief Define the event base for UMN events
 */
ESP_EVENT_DEFINE_BASE(UMNI_EVENT_BASE);

/**
 * @brief Initialize the event bus
 * 
 * Creates the default event loop if not already created.
 * 
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_events_init(void) {
    esp_err_t ret = esp_event_loop_create_default();
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Event bus initialized successfully");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "Event bus already initialized");
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to initialize event bus: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Publish an event to the event bus
 * 
 * @param event_id Event ID to publish
 * @param event_data Optional event data (NULL if none)
 * @param event_data_size Size of event data in bytes (0 if no data)
 * @param ticks_to_wait Number of ticks to wait for posting
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_event_publish(int32_t event_id, 
                           void* event_data, 
                           size_t event_data_size, 
                           TickType_t ticks_to_wait) {
    
    if (event_id < 0 && event_id != UMNI_EVENT_ANY) {
        ESP_LOGE(TAG, "Invalid event ID: %ld", (long)event_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_event_post(UMNI_EVENT_BASE, 
                                   event_id, 
                                   event_data, 
                                   event_data_size, 
                                   ticks_to_wait);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish event %ld: %s", 
                (long)event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Subscribe to a specific event
 * 
 * @param event_id Event ID to subscribe to (use UMNI_EVENT_ANY for all events)
 * @param event_handler Handler function to call when event occurs
 * @param handler_arg Optional argument passed to handler
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_event_subscribe(int32_t event_id, 
                             um_event_handler_t event_handler, 
                             void* handler_arg) {
    
    if (event_handler == NULL) {
        ESP_LOGE(TAG, "Event handler cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_event_handler_register(UMNI_EVENT_BASE, 
                                               event_id, 
                                               event_handler, 
                                               handler_arg);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Successfully subscribed to event %ld", (long)event_id);
    } else {
        ESP_LOGE(TAG, "Failed to subscribe to event %ld: %s", 
                (long)event_id, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Unsubscribe from an event
 * 
 * @param event_id Event ID to unsubscribe from
 * @param event_handler Handler function to remove
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t um_event_unsubscribe(int32_t event_id, 
                               um_event_handler_t event_handler) {
    
    if (event_handler == NULL) {
        ESP_LOGE(TAG, "Event handler cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t ret = esp_event_handler_unregister(UMNI_EVENT_BASE, 
                                                 event_id, 
                                                 event_handler);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Successfully unsubscribed from event %ld", (long)event_id);
    } else {
        ESP_LOGE(TAG, "Failed to unsubscribe from event %ld: %s", 
                (long)event_id, esp_err_to_name(ret));
    }
    
    return ret;
}