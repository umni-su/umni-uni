#include <string.h>
#include <math.h>
#include "um_automations.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "um_storage.h"
#include "um_events.h"
#include "um_capabilities.h"
#include "um_dio.h"
#include "um_opencollectors.h"
#include "um_opentherm.h"

static const char *TAG = "um_automations";

#define AUTOMATIONS_FILE "/spiffs/automations.json"

static um_automation_rule_t s_rules[UM_AUTOMATIONS_MAX_RULES];
static uint8_t s_rule_count = 0;

// Вспомогательные функции
static um_automation_op_t op_from_string(const char *op_str)
{
    if (strcmp(op_str, ">") == 0)
        return UM_AUTO_OP_GREATER;
    if (strcmp(op_str, "<") == 0)
        return UM_AUTO_OP_LESS;
    if (strcmp(op_str, ">=") == 0)
        return UM_AUTO_OP_GREATER_EQUAL;
    if (strcmp(op_str, "<=") == 0)
        return UM_AUTO_OP_LESS_EQUAL;
    return UM_AUTO_OP_EQUAL;
}

static const char *op_to_string(um_automation_op_t op)
{
    switch (op)
    {
    case UM_AUTO_OP_GREATER:
        return ">";
    case UM_AUTO_OP_LESS:
        return "<";
    case UM_AUTO_OP_GREATER_EQUAL:
        return ">=";
    case UM_AUTO_OP_LESS_EQUAL:
        return "<=";
    default:
        return "==";
    }
}

static bool evaluate_condition(um_automation_op_t op, double sensor_value, double threshold)
{
    switch (op)
    {
    case UM_AUTO_OP_EQUAL:
        return fabs(sensor_value - threshold) < 1e-6;
    case UM_AUTO_OP_GREATER:
        return sensor_value > threshold;
    case UM_AUTO_OP_LESS:
        return sensor_value < threshold;
    case UM_AUTO_OP_GREATER_EQUAL:
        return sensor_value >= threshold;
    case UM_AUTO_OP_LESS_EQUAL:
        return sensor_value <= threshold;
    default:
        return false;
    }
}

// Выполнение одного действия
static void execute_action(uint32_t cap, const char *subtype, uint8_t action)
{
    // Outputs 1-8
    if (cap >= UM_CAP_OUT1 && cap <= UM_CAP_OUT8)
    {
        uint8_t idx = cap - UM_CAP_OUT1;
        um_dio_set_output((um_do_port_index_t)idx, action ? DO_HIGH : DO_LOW);
        ESP_LOGI(TAG, "Set output %d to %d", idx, action);
        return;
    }
    // OpenCollectors
    if (cap == UM_CAP_OC1)
    {
        um_opencollectors_set(UM_OC_CHANNEL_1, action ? UM_OC_STATE_ON : UM_OC_STATE_OFF);
        ESP_LOGI(TAG, "Set OC1 to %d", action);
        return;
    }
    if (cap == UM_CAP_OC2)
    {
        um_opencollectors_set(UM_OC_CHANNEL_2, action ? UM_OC_STATE_ON : UM_OC_STATE_OFF);
        ESP_LOGI(TAG, "Set OC2 to %d", action);
        return;
    }
    // OpenTherm
    if (cap == UM_CAP_OPENTHERM)
    {
        if (subtype && strcmp(subtype, "ch") == 0)
        {
            um_ot_set_ch_en(action ? true : false);
            ESP_LOGI(TAG, "Set OpenTherm CH to %d", action);
        }
        else if (subtype && strcmp(subtype, "dhw") == 0)
        {
            um_ot_set_dhw_en(action ? true : false);
            ESP_LOGI(TAG, "Set OpenTherm DHW to %d", action);
        }
        else
        {
            ESP_LOGW(TAG, "Unknown OpenTherm subtype: %s", subtype ? subtype : "NULL");
        }
        return;
    }
    ESP_LOGW(TAG, "Unsupported action capability: %s", um_capabilities_get_name(cap));
}

// Проверка и выполнение правил
static void check_automations(um_capability_t cap, const char *subtype, double value)
{
    for (int i = 0; i < s_rule_count; i++)
    {
        um_automation_rule_t *rule = &s_rules[i];

        // Проверяем capability
        if (rule->condition_cap != (uint32_t)cap)
            continue;

        // Проверяем subtype (если в правиле задан)
        if (strlen(rule->condition_subtype) > 0)
        {
            if (subtype == NULL || strlen(subtype) == 0)
            {
                continue; // В правиле есть subtype, но в событии нет
            }
            if (strcmp(rule->condition_subtype, subtype) != 0)
            {
                continue; // subtype не совпадает
            }
        }

        // Проверяем значение
        bool condition_true = evaluate_condition(rule->op, value, rule->value);

        ESP_LOGI(TAG, "Rule %d: cap=%d subtype=%s value=%.2f th=%.2f -> %s",
                 rule->id, cap, subtype ? subtype : "NULL",
                 value, rule->value, condition_true ? "true" : "false");

        if (condition_true)
        {
            for (int j = 0; j < rule->then_count; j++)
            {
                execute_action(rule->then_actions[j].cap,
                               rule->then_actions[j].subtype,
                               rule->then_actions[j].action);
            }
        }
        else if (rule->else_count > 0)
        {
            for (int j = 0; j < rule->else_count; j++)
            {
                execute_action(rule->else_actions[j].cap,
                               rule->else_actions[j].subtype,
                               rule->else_actions[j].action);
            }
        }
    }
}

// Обработчик событий
static void automations_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == UMNI_EVENT_SENSOR_CHANGED)
    {
        um_event_sensor_payload_t *payload = (um_event_sensor_payload_t *)data;
        if (payload)
        {
            check_automations(
                (um_capability_t)payload->capability,
                payload->serial,
                payload->value);
        }
    }
}

// Загрузка правил из файла
static esp_err_t load_rules_from_file(void)
{
    char *json_str = um_storage_read_json_string(AUTOMATIONS_FILE);
    if (json_str == NULL)
    {
        ESP_LOGI(TAG, "No automations file, starting empty");
        s_rule_count = 0;
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse automations JSON");
        s_rule_count = 0;
        return ESP_FAIL;
    }

    if (!cJSON_IsArray(root))
    {
        ESP_LOGE(TAG, "Automations JSON is not an array");
        cJSON_Delete(root);
        s_rule_count = 0;
        return ESP_FAIL;
    }

    int array_size = cJSON_GetArraySize(root);
    if (array_size > UM_AUTOMATIONS_MAX_RULES)
    {
        ESP_LOGW(TAG, "Too many rules (%d), truncating to %d", array_size, UM_AUTOMATIONS_MAX_RULES);
        array_size = UM_AUTOMATIONS_MAX_RULES;
    }

    s_rule_count = 0;
    for (int i = 0; i < array_size; i++)
    {
        cJSON *rule_obj = cJSON_GetArrayItem(root, i);
        if (!rule_obj)
            continue;

        um_automation_rule_t rule = {0};

        // id
        cJSON *id_json = cJSON_GetObjectItem(rule_obj, "id");
        rule.id = cJSON_IsNumber(id_json) ? id_json->valueint : i + 1;

        // if
        cJSON *if_obj = cJSON_GetObjectItem(rule_obj, "if");
        if (!if_obj)
        {
            ESP_LOGW(TAG, "Rule %d missing 'if'", rule.id);
            continue;
        }

        cJSON *cap_json = cJSON_GetObjectItem(if_obj, "capability");
        cJSON *op_json = cJSON_GetObjectItem(if_obj, "op");
        cJSON *val_json = cJSON_GetObjectItem(if_obj, "value");

        if (!cap_json || !cJSON_IsString(cap_json) ||
            !op_json || !cJSON_IsString(op_json) ||
            !val_json || !cJSON_IsNumber(val_json))
        {
            ESP_LOGW(TAG, "Rule %d invalid 'if' fields", rule.id);
            continue;
        }

        rule.condition_cap = um_capabilities_get_by_name(cap_json->valuestring);
        if (rule.condition_cap == UM_CAP_NONE)
        {
            ESP_LOGW(TAG, "Rule %d unknown capability '%s'", rule.id, cap_json->valuestring);
            continue;
        }

        cJSON *subtype_json = cJSON_GetObjectItem(if_obj, "subtype");
        if (subtype_json && cJSON_IsString(subtype_json))
        {
            strncpy(rule.condition_subtype, subtype_json->valuestring, sizeof(rule.condition_subtype) - 1);
            rule.condition_subtype[sizeof(rule.condition_subtype) - 1] = 0;
        }
        else
        {
            rule.condition_subtype[0] = 0;
        }

        rule.op = op_from_string(op_json->valuestring);
        rule.value = val_json->valuedouble;

        // then
        cJSON *then_arr = cJSON_GetObjectItem(rule_obj, "then");
        if (!then_arr || !cJSON_IsArray(then_arr))
        {
            ESP_LOGW(TAG, "Rule %d missing 'then' array", rule.id);
            continue;
        }

        int then_size = cJSON_GetArraySize(then_arr);
        if (then_size > UM_AUTOMATIONS_MAX_ACTIONS)
            then_size = UM_AUTOMATIONS_MAX_ACTIONS;
        rule.then_count = 0;

        for (int j = 0; j < then_size; j++)
        {
            cJSON *act = cJSON_GetArrayItem(then_arr, j);
            if (!act)
                continue;

            cJSON *act_cap = cJSON_GetObjectItem(act, "capability");
            cJSON *act_act = cJSON_GetObjectItem(act, "action");
            if (!act_cap || !cJSON_IsString(act_cap) || !act_act || !cJSON_IsNumber(act_act))
                continue;

            uint32_t cap_act = um_capabilities_get_by_name(act_cap->valuestring);
            if (cap_act == UM_CAP_NONE)
                continue;

            rule.then_actions[rule.then_count].cap = cap_act;

            cJSON *act_sub = cJSON_GetObjectItem(act, "subtype");
            if (act_sub && cJSON_IsString(act_sub))
            {
                strncpy(rule.then_actions[rule.then_count].subtype, act_sub->valuestring,
                        sizeof(rule.then_actions[0].subtype) - 1);
                rule.then_actions[rule.then_count].subtype[sizeof(rule.then_actions[0].subtype) - 1] = 0;
            }
            else
            {
                rule.then_actions[rule.then_count].subtype[0] = 0;
            }

            rule.then_actions[rule.then_count].action = (uint8_t)act_act->valueint;
            rule.then_count++;
        }

        // else (опционально)
        cJSON *else_arr = cJSON_GetObjectItem(rule_obj, "else");
        if (else_arr && cJSON_IsArray(else_arr))
        {
            int else_size = cJSON_GetArraySize(else_arr);
            if (else_size > UM_AUTOMATIONS_MAX_ACTIONS)
                else_size = UM_AUTOMATIONS_MAX_ACTIONS;
            rule.else_count = 0;

            for (int j = 0; j < else_size; j++)
            {
                cJSON *act = cJSON_GetArrayItem(else_arr, j);
                if (!act)
                    continue;

                cJSON *act_cap = cJSON_GetObjectItem(act, "capability");
                cJSON *act_act = cJSON_GetObjectItem(act, "action");
                if (!act_cap || !cJSON_IsString(act_cap) || !act_act || !cJSON_IsNumber(act_act))
                    continue;

                uint32_t cap_act = um_capabilities_get_by_name(act_cap->valuestring);
                if (cap_act == UM_CAP_NONE)
                    continue;

                rule.else_actions[rule.else_count].cap = cap_act;

                cJSON *act_sub = cJSON_GetObjectItem(act, "subtype");
                if (act_sub && cJSON_IsString(act_sub))
                {
                    strncpy(rule.else_actions[rule.else_count].subtype, act_sub->valuestring,
                            sizeof(rule.else_actions[0].subtype) - 1);
                    rule.else_actions[rule.else_count].subtype[sizeof(rule.else_actions[0].subtype) - 1] = 0;
                }
                else
                {
                    rule.else_actions[rule.else_count].subtype[0] = 0;
                }

                rule.else_actions[rule.else_count].action = (uint8_t)act_act->valueint;
                rule.else_count++;
            }
        }
        else
        {
            rule.else_count = 0;
        }

        s_rules[s_rule_count++] = rule;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d automations rules", s_rule_count);
    return ESP_OK;
}

// Сохранение правил в файл
esp_err_t um_automations_save(void)
{
    cJSON *root = cJSON_CreateArray();
    if (root == NULL)
        return ESP_ERR_NO_MEM;

    for (int i = 0; i < s_rule_count; i++)
    {
        um_automation_rule_t *r = &s_rules[i];
        cJSON *rule_obj = cJSON_CreateObject();
        if (!rule_obj)
            continue;

        cJSON_AddNumberToObject(rule_obj, "id", r->id);

        // if
        cJSON *if_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(if_obj, "capability", um_capabilities_get_name(r->condition_cap));
        if (strlen(r->condition_subtype) > 0)
        {
            cJSON_AddStringToObject(if_obj, "subtype", r->condition_subtype);
        }
        cJSON_AddStringToObject(if_obj, "op", op_to_string(r->op));
        cJSON_AddNumberToObject(if_obj, "value", r->value);
        cJSON_AddItemToObject(rule_obj, "if", if_obj);

        // then
        cJSON *then_arr = cJSON_CreateArray();
        for (int j = 0; j < r->then_count; j++)
        {
            cJSON *act = cJSON_CreateObject();
            cJSON_AddStringToObject(act, "capability", um_capabilities_get_name(r->then_actions[j].cap));
            if (strlen(r->then_actions[j].subtype) > 0)
                cJSON_AddStringToObject(act, "subtype", r->then_actions[j].subtype);
            cJSON_AddNumberToObject(act, "action", r->then_actions[j].action);
            cJSON_AddItemToArray(then_arr, act);
        }
        cJSON_AddItemToObject(rule_obj, "then", then_arr);

        // else
        if (r->else_count > 0)
        {
            cJSON *else_arr = cJSON_CreateArray();
            for (int j = 0; j < r->else_count; j++)
            {
                cJSON *act = cJSON_CreateObject();
                cJSON_AddStringToObject(act, "capability", um_capabilities_get_name(r->else_actions[j].cap));
                if (strlen(r->else_actions[j].subtype) > 0)
                    cJSON_AddStringToObject(act, "subtype", r->else_actions[j].subtype);
                cJSON_AddNumberToObject(act, "action", r->else_actions[j].action);
                cJSON_AddItemToArray(else_arr, act);
            }
            cJSON_AddItemToObject(rule_obj, "else", else_arr);
        }

        cJSON_AddItemToArray(root, rule_obj);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL)
        return ESP_ERR_NO_MEM;

    esp_err_t err = um_storage_write_json(AUTOMATIONS_FILE, json_str);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Saved %d rules", s_rule_count);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to write automations file");
    }
    free(json_str);
    return err;
}

esp_err_t um_automations_reload(void)
{
    return load_rules_from_file();
}

esp_err_t um_automations_add_rule(cJSON *rule_json)
{
    if (s_rule_count >= UM_AUTOMATIONS_MAX_RULES)
    {
        ESP_LOGE(TAG, "Max rules reached (%d)", UM_AUTOMATIONS_MAX_RULES);
        return ESP_ERR_NO_MEM;
    }

    um_automation_rule_t new_rule = {0};

    // ID
    cJSON *id_json = cJSON_GetObjectItem(rule_json, "id");
    if (cJSON_IsNumber(id_json))
    {
        new_rule.id = id_json->valueint;
    }
    else
    {
        uint32_t max_id = 0;
        for (int i = 0; i < s_rule_count; i++)
        {
            if (s_rules[i].id > max_id)
                max_id = s_rules[i].id;
        }
        new_rule.id = max_id + 1;
    }

    // if
    cJSON *if_obj = cJSON_GetObjectItem(rule_json, "if");
    if (!if_obj)
    {
        ESP_LOGE(TAG, "Missing 'if' field");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *cap_json = cJSON_GetObjectItem(if_obj, "capability");
    cJSON *op_json = cJSON_GetObjectItem(if_obj, "op");
    cJSON *val_json = cJSON_GetObjectItem(if_obj, "value");

    if (!cap_json || !op_json || !val_json)
    {
        ESP_LOGE(TAG, "Invalid 'if' fields");
        return ESP_ERR_INVALID_ARG;
    }

    new_rule.condition_cap = um_capabilities_get_by_name(cap_json->valuestring);
    if (new_rule.condition_cap == UM_CAP_NONE)
    {
        ESP_LOGE(TAG, "Unknown capability: %s", cap_json->valuestring);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *subtype_json = cJSON_GetObjectItem(if_obj, "subtype");
    if (subtype_json && cJSON_IsString(subtype_json))
    {
        strncpy(new_rule.condition_subtype, subtype_json->valuestring, sizeof(new_rule.condition_subtype) - 1);
        new_rule.condition_subtype[sizeof(new_rule.condition_subtype) - 1] = 0;
    }
    else
    {
        new_rule.condition_subtype[0] = 0;
    }

    new_rule.op = op_from_string(op_json->valuestring);
    new_rule.value = val_json->valuedouble;

    // then
    cJSON *then_arr = cJSON_GetObjectItem(rule_json, "then");
    if (!then_arr || !cJSON_IsArray(then_arr))
    {
        ESP_LOGE(TAG, "Missing or invalid 'then' array");
        return ESP_ERR_INVALID_ARG;
    }

    int then_size = cJSON_GetArraySize(then_arr);
    if (then_size > UM_AUTOMATIONS_MAX_ACTIONS)
        then_size = UM_AUTOMATIONS_MAX_ACTIONS;
    new_rule.then_count = 0;

    for (int j = 0; j < then_size; j++)
    {
        cJSON *act = cJSON_GetArrayItem(then_arr, j);
        if (!act)
            continue;

        cJSON *act_cap = cJSON_GetObjectItem(act, "capability");
        cJSON *act_act = cJSON_GetObjectItem(act, "action");
        if (!act_cap || !act_act)
            continue;

        uint32_t cap = um_capabilities_get_by_name(act_cap->valuestring);
        if (cap == UM_CAP_NONE)
            continue;

        new_rule.then_actions[new_rule.then_count].cap = cap;

        cJSON *act_sub = cJSON_GetObjectItem(act, "subtype");
        if (act_sub && cJSON_IsString(act_sub))
        {
            strncpy(new_rule.then_actions[new_rule.then_count].subtype, act_sub->valuestring,
                    sizeof(new_rule.then_actions[0].subtype) - 1);
            new_rule.then_actions[new_rule.then_count].subtype[sizeof(new_rule.then_actions[0].subtype) - 1] = 0;
        }
        else
        {
            new_rule.then_actions[new_rule.then_count].subtype[0] = 0;
        }

        new_rule.then_actions[new_rule.then_count].action = (uint8_t)act_act->valueint;
        new_rule.then_count++;
    }

    if (new_rule.then_count == 0)
    {
        ESP_LOGE(TAG, "No valid actions in 'then' array");
        return ESP_ERR_INVALID_ARG;
    }

    // else (опционально)
    cJSON *else_arr = cJSON_GetObjectItem(rule_json, "else");
    if (else_arr && cJSON_IsArray(else_arr))
    {
        int else_size = cJSON_GetArraySize(else_arr);
        if (else_size > UM_AUTOMATIONS_MAX_ACTIONS)
            else_size = UM_AUTOMATIONS_MAX_ACTIONS;
        new_rule.else_count = 0;

        for (int j = 0; j < else_size; j++)
        {
            cJSON *act = cJSON_GetArrayItem(else_arr, j);
            if (!act)
                continue;

            cJSON *act_cap = cJSON_GetObjectItem(act, "capability");
            cJSON *act_act = cJSON_GetObjectItem(act, "action");
            if (!act_cap || !act_act)
                continue;

            uint32_t cap = um_capabilities_get_by_name(act_cap->valuestring);
            if (cap == UM_CAP_NONE)
                continue;

            new_rule.else_actions[new_rule.else_count].cap = cap;

            cJSON *act_sub = cJSON_GetObjectItem(act, "subtype");
            if (act_sub && cJSON_IsString(act_sub))
            {
                strncpy(new_rule.else_actions[new_rule.else_count].subtype, act_sub->valuestring,
                        sizeof(new_rule.else_actions[0].subtype) - 1);
                new_rule.else_actions[new_rule.else_count].subtype[sizeof(new_rule.else_actions[0].subtype) - 1] = 0;
            }
            else
            {
                new_rule.else_actions[new_rule.else_count].subtype[0] = 0;
            }

            new_rule.else_actions[new_rule.else_count].action = (uint8_t)act_act->valueint;
            new_rule.else_count++;
        }
    }
    else
    {
        new_rule.else_count = 0;
    }

    s_rules[s_rule_count++] = new_rule;
    ESP_LOGI(TAG, "Added rule ID=%d, now %d rules", new_rule.id, s_rule_count);

    return um_automations_save();
}

esp_err_t um_automations_remove_rule(uint32_t id)
{
    int idx = -1;
    for (int i = 0; i < s_rule_count; i++)
    {
        if (s_rules[i].id == id)
        {
            idx = i;
            break;
        }
    }

    if (idx == -1)
    {
        ESP_LOGW(TAG, "Rule ID=%d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    for (int i = idx; i < s_rule_count - 1; i++)
    {
        s_rules[i] = s_rules[i + 1];
    }
    s_rule_count--;

    ESP_LOGI(TAG, "Removed rule ID=%d, now %d rules", id, s_rule_count);
    return um_automations_save();
}

esp_err_t um_automations_update_rule(uint32_t id, cJSON *rule_json)
{
    int idx = -1;
    for (int i = 0; i < s_rule_count; i++)
    {
        if (s_rules[i].id == id)
        {
            idx = i;
            break;
        }
    }

    if (idx == -1)
    {
        ESP_LOGW(TAG, "Rule ID=%d not found", id);
        return ESP_ERR_NOT_FOUND;
    }

    um_automation_rule_t new_rule = {0};
    new_rule.id = id;

    // if
    cJSON *if_obj = cJSON_GetObjectItem(rule_json, "if");
    if (!if_obj)
        return ESP_ERR_INVALID_ARG;

    cJSON *cap_json = cJSON_GetObjectItem(if_obj, "capability");
    cJSON *op_json = cJSON_GetObjectItem(if_obj, "op");
    cJSON *val_json = cJSON_GetObjectItem(if_obj, "value");

    if (!cap_json || !op_json || !val_json)
        return ESP_ERR_INVALID_ARG;

    new_rule.condition_cap = um_capabilities_get_by_name(cap_json->valuestring);
    if (new_rule.condition_cap == UM_CAP_NONE)
        return ESP_ERR_INVALID_ARG;

    cJSON *subtype_json = cJSON_GetObjectItem(if_obj, "subtype");
    if (subtype_json && cJSON_IsString(subtype_json))
    {
        strncpy(new_rule.condition_subtype, subtype_json->valuestring, sizeof(new_rule.condition_subtype) - 1);
        new_rule.condition_subtype[sizeof(new_rule.condition_subtype) - 1] = 0;
    }
    else
    {
        new_rule.condition_subtype[0] = 0;
    }

    new_rule.op = op_from_string(op_json->valuestring);
    new_rule.value = val_json->valuedouble;

    // then
    cJSON *then_arr = cJSON_GetObjectItem(rule_json, "then");
    if (!then_arr || !cJSON_IsArray(then_arr))
        return ESP_ERR_INVALID_ARG;

    int then_size = cJSON_GetArraySize(then_arr);
    if (then_size > UM_AUTOMATIONS_MAX_ACTIONS)
        then_size = UM_AUTOMATIONS_MAX_ACTIONS;
    new_rule.then_count = 0;

    for (int j = 0; j < then_size; j++)
    {
        cJSON *act = cJSON_GetArrayItem(then_arr, j);
        if (!act)
            continue;

        cJSON *act_cap = cJSON_GetObjectItem(act, "capability");
        cJSON *act_act = cJSON_GetObjectItem(act, "action");
        if (!act_cap || !act_act)
            continue;

        uint32_t cap = um_capabilities_get_by_name(act_cap->valuestring);
        if (cap == UM_CAP_NONE)
            continue;

        new_rule.then_actions[new_rule.then_count].cap = cap;

        cJSON *act_sub = cJSON_GetObjectItem(act, "subtype");
        if (act_sub && cJSON_IsString(act_sub))
        {
            strncpy(new_rule.then_actions[new_rule.then_count].subtype, act_sub->valuestring,
                    sizeof(new_rule.then_actions[0].subtype) - 1);
            new_rule.then_actions[new_rule.then_count].subtype[sizeof(new_rule.then_actions[0].subtype) - 1] = 0;
        }
        else
        {
            new_rule.then_actions[new_rule.then_count].subtype[0] = 0;
        }

        new_rule.then_actions[new_rule.then_count].action = (uint8_t)act_act->valueint;
        new_rule.then_count++;
    }

    if (new_rule.then_count == 0)
        return ESP_ERR_INVALID_ARG;

    // else
    cJSON *else_arr = cJSON_GetObjectItem(rule_json, "else");
    if (else_arr && cJSON_IsArray(else_arr))
    {
        int else_size = cJSON_GetArraySize(else_arr);
        if (else_size > UM_AUTOMATIONS_MAX_ACTIONS)
            else_size = UM_AUTOMATIONS_MAX_ACTIONS;
        new_rule.else_count = 0;

        for (int j = 0; j < else_size; j++)
        {
            cJSON *act = cJSON_GetArrayItem(else_arr, j);
            if (!act)
                continue;

            cJSON *act_cap = cJSON_GetObjectItem(act, "capability");
            cJSON *act_act = cJSON_GetObjectItem(act, "action");
            if (!act_cap || !act_act)
                continue;

            uint32_t cap = um_capabilities_get_by_name(act_cap->valuestring);
            if (cap == UM_CAP_NONE)
                continue;

            new_rule.else_actions[new_rule.else_count].cap = cap;

            cJSON *act_sub = cJSON_GetObjectItem(act, "subtype");
            if (act_sub && cJSON_IsString(act_sub))
            {
                strncpy(new_rule.else_actions[new_rule.else_count].subtype, act_sub->valuestring,
                        sizeof(new_rule.else_actions[0].subtype) - 1);
                new_rule.else_actions[new_rule.else_count].subtype[sizeof(new_rule.else_actions[0].subtype) - 1] = 0;
            }
            else
            {
                new_rule.else_actions[new_rule.else_count].subtype[0] = 0;
            }

            new_rule.else_actions[new_rule.else_count].action = (uint8_t)act_act->valueint;
            new_rule.else_count++;
        }
    }
    else
    {
        new_rule.else_count = 0;
    }

    s_rules[idx] = new_rule;
    ESP_LOGI(TAG, "Updated rule ID=%d", id);

    return um_automations_save();
}

char *um_automations_get_config_json(void)
{
    cJSON *root = cJSON_CreateArray();
    if (!root)
        return NULL;

    for (int i = 0; i < s_rule_count; i++)
    {
        um_automation_rule_t *r = &s_rules[i];
        cJSON *rule_obj = cJSON_CreateObject();
        if (!rule_obj)
            continue;

        cJSON_AddNumberToObject(rule_obj, "id", r->id);

        cJSON *if_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(if_obj, "capability", um_capabilities_get_name(r->condition_cap));
        if (strlen(r->condition_subtype) > 0)
        {
            cJSON_AddStringToObject(if_obj, "subtype", r->condition_subtype);
        }
        cJSON_AddStringToObject(if_obj, "op", op_to_string(r->op));
        cJSON_AddNumberToObject(if_obj, "value", r->value);
        cJSON_AddItemToObject(rule_obj, "if", if_obj);

        cJSON *then_arr = cJSON_CreateArray();
        for (int j = 0; j < r->then_count; j++)
        {
            cJSON *act = cJSON_CreateObject();
            cJSON_AddStringToObject(act, "capability", um_capabilities_get_name(r->then_actions[j].cap));
            if (strlen(r->then_actions[j].subtype))
                cJSON_AddStringToObject(act, "subtype", r->then_actions[j].subtype);
            cJSON_AddNumberToObject(act, "action", r->then_actions[j].action);
            cJSON_AddItemToArray(then_arr, act);
        }
        cJSON_AddItemToObject(rule_obj, "then", then_arr);

        if (r->else_count > 0)
        {
            cJSON *else_arr = cJSON_CreateArray();
            for (int j = 0; j < r->else_count; j++)
            {
                cJSON *act = cJSON_CreateObject();
                cJSON_AddStringToObject(act, "capability", um_capabilities_get_name(r->else_actions[j].cap));
                if (strlen(r->else_actions[j].subtype))
                    cJSON_AddStringToObject(act, "subtype", r->else_actions[j].subtype);
                cJSON_AddNumberToObject(act, "action", r->else_actions[j].action);
                cJSON_AddItemToArray(else_arr, act);
            }
            cJSON_AddItemToObject(rule_obj, "else", else_arr);
        }

        cJSON_AddItemToArray(root, rule_obj);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

esp_err_t um_automations_init(void)
{
    esp_err_t err = load_rules_from_file();
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "No existing rules, starting fresh");
        s_rule_count = 0;
    }

    err = um_event_subscribe(UMNI_EVENT_SENSOR_CHANGED, automations_event_handler, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to subscribe to sensor events");
    }

    ESP_LOGI(TAG, "Automations module initialized with %d rules", s_rule_count);
    return ESP_OK;
}