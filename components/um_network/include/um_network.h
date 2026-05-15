#ifndef UM_NETWORK_H
#define UM_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif
    esp_err_t um_network_init(void);

    esp_err_t um_network_stop(void);

#ifdef __cplusplus
}
#endif

#endif