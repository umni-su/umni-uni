/**
 * @file um_events.h
 * @brief Simple event bus for ESP-IDF applications
 * @version 1.0.0
 */

#ifndef UM_EVENTS_H
#define UM_EVENTS_H

#include "esp_event.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Declare the event base for UMN events
     */
    ESP_EVENT_DECLARE_BASE(UMNI_EVENT_BASE);

    /**
     * @brief Event IDs for the UMN event base
     */
    typedef enum
    {
        UMNI_EVENT_ANY = -1, /**< Wildcard for any event */
        UMNI_EVENT_ETH_CONNECTED,
        UMNI_EVENT_ETH_DISCONNECTED,
        UMNI_EVENT_WIFI_CONNECTED,
        UMNI_EVENT_WIFI_DISCONNECTED,
        UMNI_EVENT_SDCARD_MOUNTED,
        UMNI_EVENT_SDCARD_UNMOUNTED,
        UMNI_EVENT_SDCARD_PUSH_IN,
        UMNI_EVENT_SDCARD_PUSH_OUT,
        UMNI_EVENT_OPENTHERM_CH_ON,
        UMNI_EVENT_OPENTHERM_CH_OFF,
        UMNI_EVENT_OPENTHERM_SET_DATA,
        UMNI_EVENT_MQTT_CONFIG_DIO,
        UMNI_EVENT_MQTT_CONFIG_ONEWIRE,
        UMNI_EVENT_MQTT_CONFIG_ADC,
        UMNI_EVENT_MQTT_CONFIG_NTC,
        UMNI_EVENT_MQTT_CONFIG_RF433,
        UMNI_EVENT_SENSOR_CHANGED,
        UMNI_EVENT_OUTPUT_CHANGED,
        UMNI_EVENT_INPUT_CHANGED,
        UMNI_EVENT_OPENCOLLECTOR_CHANGED,

    } umn_event_id_t;

    typedef struct
    {
        char *category;
        uint8_t capability;
        float value;
        const char *serial;
    } um_event_sensor_payload_t;

    /**
     * @brief Event handler function type
     *
     * @param event_handler_arg User argument passed during registration
     * @param event_base Event base
     * @param event_id Event ID
     * @param event_data Event data (can be NULL)
     */
    typedef void (*um_event_handler_t)(void *event_handler_arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data);

    /**
     * @brief Initialize the event bus
     *
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t um_events_init(void);

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
                               void *event_data,
                               size_t event_data_size,
                               TickType_t ticks_to_wait);

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
                                 void *handler_arg);

    /**
     * @brief Unsubscribe from an event
     *
     * @param event_id Event ID to unsubscribe from
     * @param event_handler Handler function to remove
     * @return esp_err_t ESP_OK on success, error code otherwise
     */
    esp_err_t um_event_unsubscribe(int32_t event_id,
                                   um_event_handler_t event_handler);

#ifdef __cplusplus
}
#endif

#endif // UM_EVENTS_H