#include "um_store.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "um_helpers.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "um_store";
static um_store_t s_stores[UM_STORE_MAX_STORES];
static uint8_t s_store_count = 0;

// Поиск хранилища по имени
static um_store_t *find_store(const char *name)
{
    for (int i = 0; i < s_store_count; i++)
    {
        if (strcmp(s_stores[i].name, name) == 0)
        {
            return &s_stores[i];
        }
    }
    return NULL;
}

um_store_t *um_store_find_store(const char *name)
{
    return find_store(name);
}

um_store_t *um_store_create(const char *name)
{
    if (!name)
        return NULL;

    // Проверяем, существует ли уже
    um_store_t *existing = find_store(name);
    if (existing)
    {
        return existing;
    }

    // Создаем новое
    if (s_store_count >= UM_STORE_MAX_STORES)
    {
        ESP_LOGE(TAG, "Max stores reached");
        return NULL;
    }

    um_store_t *store = &s_stores[s_store_count++];
    strncpy(store->name, name, UM_STORE_MAX_NAME_LEN - 1);
    store->head = 0;
    store->count = 0;
    store->initialized = true;

    // Пытаемся загрузить с диска
    if (um_store_load(store) != ESP_OK)
    {
        ESP_LOGI(TAG, "Created new store '%s'", name);
    }
    else
    {
        ESP_LOGI(TAG, "Loaded store '%s' with %d entries", name, store->count);
    }

    return store;
}

esp_err_t um_store_add_value_with_time(um_store_t *store, float value, uint64_t timestamp_ms)
{
    if (!store || !store->initialized)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Получаем текущее время, если timestamp = 0
    if (timestamp_ms == 0)
    {
        timestamp_ms = um_helpers_get_real_timestamp_ms();
    }

    // Добавляем запись
    store->entries[store->head].timestamp_ms = timestamp_ms;
    store->entries[store->head].value = value;

    // Сдвигаем head
    store->head = (store->head + 1) % UM_STORE_MAX_SLOTS;

    // Увеличиваем счетчик до максимума
    if (store->count < UM_STORE_MAX_SLOTS)
    {
        store->count++;
    }

    ESP_LOGD(TAG, "Store '%s': added value %.2f at %llu",
             store->name, value, timestamp_ms);

    return ESP_OK;
}

esp_err_t um_store_add_value(um_store_t *store, float value)
{
    return um_store_add_value_with_time(store, value, 0);
}

uint8_t um_store_get_all(um_store_t *store, um_store_entry_t *entries, uint8_t max_count)
{
    if (!store || !entries || max_count == 0)
        return 0;

    uint8_t to_copy = store->count < max_count ? store->count : max_count;
    uint8_t start = (store->head + UM_STORE_MAX_SLOTS - store->count) % UM_STORE_MAX_SLOTS;

    for (int i = 0; i < to_copy; i++)
    {
        uint8_t idx = (start + i) % UM_STORE_MAX_SLOTS;
        entries[i] = store->entries[idx];
    }

    return to_copy;
}

uint8_t um_store_get_last(um_store_t *store, um_store_entry_t *entries, uint8_t count)
{
    if (!store || !entries || count == 0)
        return 0;

    uint8_t to_copy = store->count < count ? store->count : count;
    uint8_t pos = store->head;

    for (int i = 0; i < to_copy; i++)
    {
        pos = (pos + UM_STORE_MAX_SLOTS - 1) % UM_STORE_MAX_SLOTS;
        entries[to_copy - 1 - i] = store->entries[pos];
    }

    return to_copy;
}

char *um_store_to_json(um_store_t *store)
{
    if (!store || store->count == 0)
    {
        return strdup("{\"error\":\"no data\"}");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *timestamps = cJSON_CreateArray();
    cJSON *values = cJSON_CreateArray();

    // Получаем все записи в правильном порядке (от старых к новым)
    um_store_entry_t entries[UM_STORE_MAX_SLOTS];
    uint8_t count = um_store_get_all(store, entries, UM_STORE_MAX_SLOTS);

    for (int i = 0; i < count; i++)
    {
        cJSON_AddItemToArray(timestamps, cJSON_CreateNumber(entries[i].timestamp_ms));
        cJSON_AddItemToArray(values, cJSON_CreateNumber(entries[i].value));
    }

    cJSON_AddStringToObject(root, "name", store->name);
    cJSON_AddItemToObject(root, "timestamps", timestamps);
    cJSON_AddItemToObject(root, "values", values);
    cJSON_AddNumberToObject(root, "count", count);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}

void um_store_stats(um_store_t *store, float *min, float *max, float *avg)
{
    if (!store || store->count == 0)
    {
        if (min)
            *min = 0;
        if (max)
            *max = 0;
        if (avg)
            *avg = 0;
        return;
    }

    float sum = 0;
    float min_val = store->entries[0].value;
    float max_val = store->entries[0].value;

    for (int i = 0; i < store->count; i++)
    {
        float val = store->entries[i].value;
        sum += val;
        if (val < min_val)
            min_val = val;
        if (val > max_val)
            max_val = val;
    }

    if (min)
        *min = min_val;
    if (max)
        *max = max_val;
    if (avg)
        *avg = sum / store->count;
}

// Опциональное сохранение на диск (заглушка)
esp_err_t um_store_save(um_store_t *store)
{
    // TODO: Реализовать сохранение в SPIFFS
    // Пока только лог
    ESP_LOGI(TAG, "Store '%s' save requested (not implemented)", store->name);
    return ESP_OK;
}

esp_err_t um_store_load(um_store_t *store)
{
    // TODO: Реализовать загрузку из SPIFFS
    // Пока просто сбрасываем
    store->head = 0;
    store->count = 0;
    return ESP_ERR_NOT_FOUND;
}

void um_store_clear(um_store_t *store)
{
    if (store)
    {
        store->head = 0;
        store->count = 0;
        ESP_LOGI(TAG, "Cleared store '%s'", store->name);
    }
}