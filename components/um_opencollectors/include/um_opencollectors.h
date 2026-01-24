#ifndef UM_OPENCOLLECTORS_H
#define UM_OPENCOLLECTORS_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_UM_FEATURE_OC1) || defined(CONFIG_UM_FEATURE_OC2)

typedef enum {
#if defined(CONFIG_UM_FEATURE_OC1)
    UM_OC_CHANNEL_1 = 0,
#endif
#if defined(CONFIG_UM_FEATURE_OC2)
    UM_OC_CHANNEL_2 = 1,
#endif
} um_oc_channel_t;

typedef enum {
    UM_OC_STATE_OFF = 0,
    UM_OC_STATE_ON = 1
} um_oc_state_t;

esp_err_t um_opencollectors_init(void);
esp_err_t um_opencollectors_set(um_oc_channel_t channel, um_oc_state_t state);
esp_err_t um_opencollectors_get(um_oc_channel_t channel, um_oc_state_t* state);
esp_err_t um_opencollectors_toggle(um_oc_channel_t channel);
esp_err_t um_opencollectors_all_off(void);

#endif // CONFIG_UM_FEATURE_OC1 || CONFIG_UM_FEATURE_OC2

#ifdef __cplusplus
}
#endif

#endif // UM_OPENCOLLECTORS_H