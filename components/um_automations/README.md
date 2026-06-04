```c
#include "um_automations.h"

void app_main(void) {
    // ... инициализация других компонентов
    um_storage_init("/spiffs", NULL, 5, true);
    um_events_init();
    um_capabilities_init();
    
    // Инициализация автоматизаций
    ESP_ERROR_CHECK(um_automations_init());
    
    // ... остальной код
}
```

Пример файла
```json
{
  "id": 1,
  "if": {
    "capability": "ntc1",
    "op": ">",
    "value": 30.0
  },
  "then": [
    { "capability": "out1", "action": 1 }
  ],
  "else": [
    { "capability": "out1", "action": 0 }
  ]
}
```

Поля условия (if)
Поле	Описание	Возможные значения
capability	Тип сенсора	ntc1, ntc2, onewire, ai1, ai2, opentherm
op	Оператор сравнения	>, <, >=, <=, ==
value	Пороговое значение	число (температура, ADC, и т.д.)
Действия (then / else)
Поле	Описание	Возможные значения
capability	Тип устройства	out1..out8, oc1, oc2, opentherm
subtype	Подтип (для opentherm)	ch (отопление), dhw (ГВС)
action	Действие	0 (выкл), 1 (вкл)
Примеры правил
1. Контроль температуры NTC
```json
{
  "if": {
    "capability": "ntc1",
    "op": ">",
    "value": 35.0
  },
  "then": [
    { "capability": "out1", "action": 1 }
  ],
  "else": [
    { "capability": "out1", "action": 0 }
  ]
}
```
2. Управление котлом по датчику 1-Wire
```json
{
  "if": {
    "capability": "onewire",
    "op": "<",
    "value": 18.0
  },
  "then": [
    { "capability": "opentherm", "subtype": "ch", "action": 1 }
  ],
  "else": [
    { "capability": "opentherm", "subtype": "ch", "action": 0 }
  ]
}
```
3. Включение вентилятора при высокой температуре
```json
{
  "if": {
    "capability": "ntc2",
    "op": ">=",
    "value": 40.0
  },
  "then": [
    { "capability": "oc1", "action": 1 },
    { "capability": "out3", "action": 1 }
  ],
  "else": [
    { "capability": "oc1", "action": 0 },
    { "capability": "out3", "action": 0 }
  ]
}
```
4. Без блока else (только включение)
```json
{
  "if": {
    "capability": "ai1",
    "op": ">",
    "value": 500
  },
  "then": [
    { "capability": "out2", "action": 1 }
  ]
}
```