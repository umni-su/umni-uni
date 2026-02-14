#include "base_config.h"
#include "um_capabilities.h"
#include "sdkconfig.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CAP_MASK(cap) (1ULL << (cap))

typedef struct
{
    um_capability_t cap;
    const char *name;
    uint64_t mask;
} capability_info_t;

// Динамический массив только с включенными фичами
static capability_info_t *s_enabled_capabilities = NULL;
static uint32_t s_enabled_count = 0;
static uint64_t s_enabled_mask = 0;

esp_err_t um_capabilities_init(void)
{
    // Освобождаем предыдущий массив если был
    if (s_enabled_capabilities)
    {
        free(s_enabled_capabilities);
        s_enabled_capabilities = NULL;
    }

    s_enabled_mask = 0;
    s_enabled_count = 0;

// Сначала подсчитаем количество включенных фич
#if UM_FEATURE_ENABLED(ETHERNET)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(WIFI)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(SDCARD)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(WEBSERVER)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(WEBHOOKS)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(MQTT)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OPENTHERM)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(RF433)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(ONEWIRE)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(ALARM)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(ADC)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(NTC1)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(NTC2)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(AI1)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(AI2)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OC1)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OC2)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(BUZZER)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INPUTS)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INP1)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INP2)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INP3)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INP4)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INP5)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(INP6)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUTPUTS)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT1)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT2)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT3)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT4)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT5)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT6)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT7)
    s_enabled_count++;
#endif
#if UM_FEATURE_ENABLED(OUT8)
    s_enabled_count++;
#endif

    if (s_enabled_count == 0)
    {
        s_enabled_capabilities = NULL;
        return ESP_OK;
    }

    // Выделяем память под массив включенных фич
    s_enabled_capabilities = malloc(s_enabled_count * sizeof(capability_info_t));
    if (!s_enabled_capabilities)
    {
        return ESP_ERR_NO_MEM;
    }

    // Заполняем массив
    uint32_t index = 0;

#if UM_FEATURE_ENABLED(ETHERNET)
    s_enabled_capabilities[index].cap = UM_CAP_ETHERNET;
    s_enabled_capabilities[index].name = "ethernet";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_ETHERNET);
    s_enabled_mask |= CAP_MASK(UM_CAP_ETHERNET);
    index++;
#endif

#if UM_FEATURE_ENABLED(WIFI)
    s_enabled_capabilities[index].cap = UM_CAP_WIFI;
    s_enabled_capabilities[index].name = "wifi";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_WIFI);
    s_enabled_mask |= CAP_MASK(UM_CAP_WIFI);
    index++;
#endif

#if UM_FEATURE_ENABLED(SDCARD)
    s_enabled_capabilities[index].cap = UM_CAP_SDCARD;
    s_enabled_capabilities[index].name = "sdcard";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_SDCARD);
    s_enabled_mask |= CAP_MASK(UM_CAP_SDCARD);
    index++;
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)
    s_enabled_capabilities[index].cap = UM_CAP_WEBSERVER;
    s_enabled_capabilities[index].name = "webserver";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_WEBSERVER);
    s_enabled_mask |= CAP_MASK(UM_CAP_WEBSERVER);
    index++;
#endif

#if UM_FEATURE_ENABLED(WEBHOOKS)
    s_enabled_capabilities[index].cap = UM_CAP_WEBHOOKS;
    s_enabled_capabilities[index].name = "webhooks";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_WEBHOOKS);
    s_enabled_mask |= CAP_MASK(UM_CAP_WEBHOOKS);
    index++;
#endif

#if UM_FEATURE_ENABLED(MQTT)
    s_enabled_capabilities[index].cap = UM_CAP_MQTT;
    s_enabled_capabilities[index].name = "mqtt";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_MQTT);
    s_enabled_mask |= CAP_MASK(UM_CAP_MQTT);
    index++;
#endif

#if UM_FEATURE_ENABLED(OPENTHERM)
    s_enabled_capabilities[index].cap = UM_CAP_OPENTHERM;
    s_enabled_capabilities[index].name = "opentherm";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OPENTHERM);
    s_enabled_mask |= CAP_MASK(UM_CAP_OPENTHERM);
    index++;
#endif

#if UM_FEATURE_ENABLED(RF433)
    s_enabled_capabilities[index].cap = UM_CAP_RF433;
    s_enabled_capabilities[index].name = "rf433";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_RF433);
    s_enabled_mask |= CAP_MASK(UM_CAP_RF433);
    index++;
#endif

#if UM_FEATURE_ENABLED(ONEWIRE)
    s_enabled_capabilities[index].cap = UM_CAP_ONEWIRE;
    s_enabled_capabilities[index].name = "onewire";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_ONEWIRE);
    s_enabled_mask |= CAP_MASK(UM_CAP_ONEWIRE);
    index++;
#endif

#if UM_FEATURE_ENABLED(ALARM)
    s_enabled_capabilities[index].cap = UM_CAP_ALARM;
    s_enabled_capabilities[index].name = "alarm";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_ALARM);
    s_enabled_mask |= CAP_MASK(UM_CAP_ALARM);
    index++;
#endif

#if UM_FEATURE_ENABLED(ADC)
    s_enabled_capabilities[index].cap = UM_CAP_ADC;
    s_enabled_capabilities[index].name = "adc";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_ADC);
    s_enabled_mask |= CAP_MASK(UM_CAP_ADC);
    index++;
#endif

#if UM_FEATURE_ENABLED(NTC1)
    s_enabled_capabilities[index].cap = UM_CAP_NTC1;
    s_enabled_capabilities[index].name = "ntc1";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_NTC1);
    s_enabled_mask |= CAP_MASK(UM_CAP_NTC1);
    index++;
#endif

#if UM_FEATURE_ENABLED(NTC2)
    s_enabled_capabilities[index].cap = UM_CAP_NTC2;
    s_enabled_capabilities[index].name = "ntc2";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_NTC2);
    s_enabled_mask |= CAP_MASK(UM_CAP_NTC2);
    index++;
#endif

#if UM_FEATURE_ENABLED(AI1)
    s_enabled_capabilities[index].cap = UM_CAP_AI1;
    s_enabled_capabilities[index].name = "ai1";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_AI1);
    s_enabled_mask |= CAP_MASK(UM_CAP_AI1);
    index++;
#endif

#if UM_FEATURE_ENABLED(AI2)
    s_enabled_capabilities[index].cap = UM_CAP_AI2;
    s_enabled_capabilities[index].name = "ai2";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_AI2);
    s_enabled_mask |= CAP_MASK(UM_CAP_AI2);
    index++;
#endif

#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
    s_enabled_capabilities[index].cap = UM_CAP_OPENCOLLECTORS;
    s_enabled_capabilities[index].name = "opencollectors";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OPENCOLLECTORS);
    s_enabled_mask |= CAP_MASK(UM_CAP_OPENCOLLECTORS);
    index++;
#endif

#if UM_FEATURE_ENABLED(OC1)
    s_enabled_capabilities[index].cap = UM_CAP_OC1;
    s_enabled_capabilities[index].name = "oc1";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OC1);
    s_enabled_mask |= CAP_MASK(UM_CAP_OC1);
    index++;
#endif

#if UM_FEATURE_ENABLED(OC2)
    s_enabled_capabilities[index].cap = UM_CAP_OC2;
    s_enabled_capabilities[index].name = "oc2";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OC2);
    s_enabled_mask |= CAP_MASK(UM_CAP_OC2);
    index++;
#endif

#if UM_FEATURE_ENABLED(BUZZER)
    s_enabled_capabilities[index].cap = UM_CAP_BUZZER;
    s_enabled_capabilities[index].name = "buzzer";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_BUZZER);
    s_enabled_mask |= CAP_MASK(UM_CAP_BUZZER);
    index++;
#endif

#if UM_FEATURE_ENABLED(INPUTS)
    s_enabled_capabilities[index].cap = UM_CAP_INPUTS;
    s_enabled_capabilities[index].name = "inputs";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INPUTS);
    s_enabled_mask |= CAP_MASK(UM_CAP_INPUTS);
    index++;
#endif

#if UM_FEATURE_ENABLED(INP1)
    s_enabled_capabilities[index].cap = UM_CAP_INP1;
    s_enabled_capabilities[index].name = "inp1";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INP1);
    s_enabled_mask |= CAP_MASK(UM_CAP_INP1);
    index++;
#endif

#if UM_FEATURE_ENABLED(INP2)
    s_enabled_capabilities[index].cap = UM_CAP_INP2;
    s_enabled_capabilities[index].name = "inp2";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INP2);
    s_enabled_mask |= CAP_MASK(UM_CAP_INP2);
    index++;
#endif

#if UM_FEATURE_ENABLED(INP3)
    s_enabled_capabilities[index].cap = UM_CAP_INP3;
    s_enabled_capabilities[index].name = "inp3";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INP3);
    s_enabled_mask |= CAP_MASK(UM_CAP_INP3);
    index++;
#endif

#if UM_FEATURE_ENABLED(INP4)
    s_enabled_capabilities[index].cap = UM_CAP_INP4;
    s_enabled_capabilities[index].name = "inp4";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INP4);
    s_enabled_mask |= CAP_MASK(UM_CAP_INP4);
    index++;
#endif

#if UM_FEATURE_ENABLED(INP5)
    s_enabled_capabilities[index].cap = UM_CAP_INP5;
    s_enabled_capabilities[index].name = "inp5";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INP5);
    s_enabled_mask |= CAP_MASK(UM_CAP_INP5);
    index++;
#endif

#if UM_FEATURE_ENABLED(INP6)
    s_enabled_capabilities[index].cap = UM_CAP_INP6;
    s_enabled_capabilities[index].name = "inp6";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_INP6);
    s_enabled_mask |= CAP_MASK(UM_CAP_INP6);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUTPUTS)
    s_enabled_capabilities[index].cap = UM_CAP_OUTPUTS;
    s_enabled_capabilities[index].name = "outputs";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUTPUTS);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUTPUTS);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT1)
    s_enabled_capabilities[index].cap = UM_CAP_OUT1;
    s_enabled_capabilities[index].name = "out1";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT1);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT1);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT2)
    s_enabled_capabilities[index].cap = UM_CAP_OUT2;
    s_enabled_capabilities[index].name = "out2";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT2);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT2);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT3)
    s_enabled_capabilities[index].cap = UM_CAP_OUT3;
    s_enabled_capabilities[index].name = "out3";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT3);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT3);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT4)
    s_enabled_capabilities[index].cap = UM_CAP_OUT4;
    s_enabled_capabilities[index].name = "out4";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT4);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT4);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT5)
    s_enabled_capabilities[index].cap = UM_CAP_OUT5;
    s_enabled_capabilities[index].name = "out5";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT5);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT5);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT6)
    s_enabled_capabilities[index].cap = UM_CAP_OUT6;
    s_enabled_capabilities[index].name = "out6";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT6);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT6);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT7)
    s_enabled_capabilities[index].cap = UM_CAP_OUT7;
    s_enabled_capabilities[index].name = "out7";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT7);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT7);
    index++;
#endif

#if UM_FEATURE_ENABLED(OUT8)
    s_enabled_capabilities[index].cap = UM_CAP_OUT8;
    s_enabled_capabilities[index].name = "out8";
    s_enabled_capabilities[index].mask = CAP_MASK(UM_CAP_OUT8);
    s_enabled_mask |= CAP_MASK(UM_CAP_OUT8);
    index++;
#endif

    return ESP_OK;
}

bool um_capabilities_has(um_capability_t cap)
{
    return (s_enabled_mask & CAP_MASK(cap)) != 0;
}

uint64_t um_capabilities_get_mask(void)
{
    return s_enabled_mask;
}

uint32_t um_capabilities_get_count(void)
{
    return s_enabled_count;
}

const char *um_capabilities_get_name(um_capability_t cap)
{
    for (uint32_t i = 0; i < s_enabled_count; i++)
    {
        if (s_enabled_capabilities[i].cap == cap)
        {
            return s_enabled_capabilities[i].name;
        }
    }
    return NULL;
}

char *um_capabilities_get_json_array(void)
{
    if (s_enabled_count == 0 || !s_enabled_capabilities)
    {
        char *json = malloc(3);
        if (!json)
            return NULL;
        strcpy(json, "[]");
        return json;
    }

    // Вычисляем размер
    size_t size = 2; // [ и ]
    for (uint32_t i = 0; i < s_enabled_count; i++)
    {
        size += strlen(s_enabled_capabilities[i].name) + 3; // "name",
    }

    char *json = malloc(size);
    if (!json)
        return NULL;

    char *ptr = json;
    ptr += sprintf(ptr, "[");

    for (uint32_t i = 0; i < s_enabled_count; i++)
    {
        if (i > 0)
        {
            ptr += sprintf(ptr, ",");
        }
        ptr += sprintf(ptr, "\"%s\"", s_enabled_capabilities[i].name);
    }

    ptr += sprintf(ptr, "]");
    return json;
}

char *um_capabilities_get_json_object(void)
{
    if (s_enabled_count == 0 || !s_enabled_capabilities)
    {
        char *json = malloc(3);
        if (!json)
            return NULL;
        strcpy(json, "{}");
        return json;
    }

    // Вычисляем размер
    size_t size = 2; // { и }
    for (uint32_t i = 0; i < s_enabled_count; i++)
    {
        size += strlen(s_enabled_capabilities[i].name) + 10; // "name":true,
    }

    char *json = malloc(size);
    if (!json)
        return NULL;

    char *ptr = json;
    ptr += sprintf(ptr, "{");

    for (uint32_t i = 0; i < s_enabled_count; i++)
    {
        if (i > 0)
        {
            ptr += sprintf(ptr, ",");
        }
        ptr += sprintf(ptr, "\"%s\":true", s_enabled_capabilities[i].name);
    }

    ptr += sprintf(ptr, "}");
    return json;
}