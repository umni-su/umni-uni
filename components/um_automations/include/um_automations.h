#ifndef UM_AUTOMATIONS_H
#define UM_AUTOMATIONS_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UM_AUTOMATIONS_MAX_RULES 20  // Максимум правил
#define UM_AUTOMATIONS_MAX_ACTIONS 5 // Максимум действий на правило

    typedef enum
    {
        UM_AUTO_OP_EQUAL,
        UM_AUTO_OP_GREATER,
        UM_AUTO_OP_LESS,
        UM_AUTO_OP_GREATER_EQUAL,
        UM_AUTO_OP_LESS_EQUAL
    } um_automation_op_t;

    typedef struct
    {
        uint32_t id;                // Идентификатор правила
        uint32_t condition_cap;     // UM_CAP_* (ntc1, onewire, rf433, opentherm)
        char condition_subtype[32]; // Опционально: serial для onewire/rf433, "ch"/"dhw" для opentherm
        um_automation_op_t op;      // Оператор сравнения
        double value;               // Пороговое значение
        // Действия THEN
        uint8_t then_count;
        struct
        {
            uint32_t cap;    // UM_CAP_* (out1, oc1, opentherm)
            char subtype[8]; // Для opentherm: "ch" или "dhw", иначе пусто
            uint8_t action;  // 0 или 1
        } then_actions[UM_AUTOMATIONS_MAX_ACTIONS];
        // Действия ELSE
        uint8_t else_count;
        struct
        {
            uint32_t cap;
            char subtype[8];
            uint8_t action;
        } else_actions[UM_AUTOMATIONS_MAX_ACTIONS];
    } um_automation_rule_t;

    /**
     * @brief Инициализация модуля автоматизаций
     *        Загружает правила из файла, подписывается на события.
     * @return ESP_OK или код ошибки
     */
    esp_err_t um_automations_init(void);

    /**
     * @brief Перезагрузить правила из файла
     */
    esp_err_t um_automations_reload(void);

    /**
     * @brief Сохранить текущие правила в файл
     */
    esp_err_t um_automations_save(void);

    /**
     * @brief Добавить новое правило из JSON-объекта
     * @param rule_json JSON с полями: if, then, (else)
     * @return ESP_OK или код ошибки
     */
    esp_err_t um_automations_add_rule(cJSON *rule_json);

    /**
     * @brief Удалить правило по ID
     */
    esp_err_t um_automations_remove_rule(uint32_t id);

    /**
     * @brief Обновить существующее правило
     * @param id ID правила
     * @param rule_json Новые данные
     */
    esp_err_t um_automations_update_rule(uint32_t id, cJSON *rule_json);

    /**
     * @brief Получить JSON со всеми правилами (для API)
     * @return Строка JSON, нужно освободить free()
     */
    char *um_automations_get_config_json(void);

#ifdef __cplusplus
}
#endif

#endif // UM_AUTOMATIONS_H