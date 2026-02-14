#ifndef UM_CAPABILITIES_H
#define UM_CAPABILITIES_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Feature flags enumeration
     */
    typedef enum
    {
        UM_CAP_NONE = 0,

        // System features
        UM_CAP_ETHERNET,
        UM_CAP_WIFI,
        UM_CAP_SDCARD,
        UM_CAP_WEBSERVER,
        UM_CAP_WEBHOOKS,
        UM_CAP_MQTT,

        // Communication protocols
        UM_CAP_OPENTHERM,
        UM_CAP_RF433,
        UM_CAP_ONEWIRE,

        // Security
        UM_CAP_ALARM,

        // Analog inputs
        UM_CAP_ADC,
        UM_CAP_NTC1,
        UM_CAP_NTC2,
        UM_CAP_AI1,
        UM_CAP_AI2,

        // Outputs
        UM_CAP_OPENCOLLECTORS,
        UM_CAP_OC1,
        UM_CAP_OC2,
        UM_CAP_BUZZER,

        // Digital inputs
        UM_CAP_INPUTS,
        UM_CAP_INP1,
        UM_CAP_INP2,
        UM_CAP_INP3,
        UM_CAP_INP4,
        UM_CAP_INP5,
        UM_CAP_INP6,

        // Digital outputs
        UM_CAP_OUTPUTS,
        UM_CAP_OUT1,
        UM_CAP_OUT2,
        UM_CAP_OUT3,
        UM_CAP_OUT4,
        UM_CAP_OUT5,
        UM_CAP_OUT6,
        UM_CAP_OUT7,
        UM_CAP_OUT8,

        UM_CAP_MAX,
        UM_CAP_COUNT = UM_CAP_MAX
    } um_capability_t;

    /**
     * @brief Initialize capabilities module
     * @return ESP_OK on success
     */
    esp_err_t um_capabilities_init(void);

    /**
     * @brief Check if a specific capability is enabled
     * @param cap Capability to check
     * @return true if enabled, false otherwise
     */
    bool um_capabilities_has(um_capability_t cap);

    /**
     * @brief Get bitmask of all enabled capabilities
     * @return 64-bit bitmask (supports up to 64 features)
     */
    uint64_t um_capabilities_get_mask(void);

    /**
     * @brief Get number of enabled capabilities
     * @return Count of enabled features
     */
    uint32_t um_capabilities_get_count(void);

    /**
     * @brief Get capability name string
     * @param cap Capability
     * @return Constant string with capability name
     */
    const char *um_capabilities_get_name(um_capability_t cap);

    /**
     * @brief Generate JSON string with all capabilities as array
     * @return JSON string (must be freed by caller)
     */
    char *um_capabilities_get_json_array(void);

    /**
     * @brief Generate JSON string with all capabilities as object
     * @return JSON string (must be freed by caller)
     */
    char *um_capabilities_get_json_object(void);

    /**
     * @brief Fast check for any capability in mask
     * @param mask Capability mask to check against
     * @return true if any capability in mask is enabled
     */
    static inline bool um_capabilities_has_any(uint64_t mask)
    {
        return (um_capabilities_get_mask() & mask) != 0;
    }

#ifdef __cplusplus
}
#endif

#endif // UM_CAPABILITIES_H