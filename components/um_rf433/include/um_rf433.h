#ifndef UM_RF433_H
#define UM_RF433_H

#include "base_config.h"
#include "rf433_receiver.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UM_RF433_MAX_SENSORS 32

#define UM_RF433_MAX_SEARCH_SENSORS 5

    typedef struct
    {
        uint32_t serial;
        long time;
        long last_processed_time; // Время последней обработки
        bool alarm;
        bool triggered;
        uint8_t state;
        uint8_t packet_count; // Счетчик пакетов для дебаунсинга
    } um_rf_devices_t;

    void um_rf_433_init();

    void um_rf433_clear_search();

    short int um_rf433_get_array_length(um_rf_devices_t *devices, int max);

    short int um_rf433_get_existing_index(um_rf_devices_t *devices, uint32_t number, int max);

#ifdef __cplusplus
}
#endif

#endif // UM_RF433_H