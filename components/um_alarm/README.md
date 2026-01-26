# UM Alarm Input

Компонент для обработки сигнала тревоги от подачи на клеммы 12V с прерыванием.

## Особенности

- Обработка прерываний на GPIO
- Поддержка фронтов: FALLING, RISING, BOTH
- Callback для событий
- Счетчик срабатываний
- ISR в IRAM, обработка в задаче FreeRTOS

## Использование

### 1. Kconfig настройки:
```
CONFIG_UM_FEATURE_ALARM=y
CONFIG_UM_CFG_ALARM_GPIO=15
```

### 2. Базовый пример:
```c
#include "um_alarm.h"

// Callback для событий
void alarm_callback(bool state, void* user_data) {
    ESP_LOGI("ALARM", "State: %s", state ? "HIGH" : "LOW");
}

void app_main() {
    // Инициализация (FALLING edge, pull-up включен)
    um_alarm_init(UM_ALARM_EDGE_FALLING, true, false);
    
    // Установка callback
    um_alarm_set_callback(alarm_callback, NULL);
    
    // Проверка состояния
    bool current_state;
    um_alarm_get_state(&current_state);
    
    // Получение счетчика
    uint32_t count;
    um_alarm_get_count(&count);
    
    // Сброс счетчика
    um_alarm_reset_count();
}
```
3. Пример с разными фронтами:
```c
// Реагировать на падающий фронт (HIGH->LOW)
um_alarm_init(UM_ALARM_EDGE_FALLING, true, false);

// Реагировать на нарастающий фронт (LOW->HIGH)  
um_alarm_init(UM_ALARM_EDGE_RISING, false, true);

// Реагировать на оба фронта
um_alarm_init(UM_ALARM_EDGE_BOTH, true, false);
```