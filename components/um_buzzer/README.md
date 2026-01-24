# UM Buzzer

Простой компонент для управления пьезо-зуммером/бипером.

## Быстрый старт

1. **Добавьте в Kconfig проекта:**
```
CONFIG_UM_FEATURE_BUZZER=y
CONFIG_UM_CFG_BUZZER_GPIO=13
```
2. **Используйте в коде:**

```c
#include "um_buzzer.h"

// Инициализация
um_buzzer_init();

// Включить/выключить
um_buzzer_set(UM_BUZZER_ON);  // Включить
um_buzzer_set(UM_BUZZER_OFF); // Выключить

// Переключить
um_buzzer_toggle();

// Получить состояние
um_buzzer_state_t state;
um_buzzer_get(&state);

// Проиграть паттерн (3 коротких бипа)
um_buzzer_beep(3, 100, 50);
```

```c
// Один длинный бип (сигнал)
um_buzzer_beep(1, 500, 0);

// 3 коротких (предупреждение)
um_buzzer_beep(3, 100, 50);

// SOS сигнал
um_buzzer_beep(3, 100, 50); // ...
vTaskDelay(pdMS_TO_TICKS(200));
um_buzzer_beep(3, 300, 100); // ---
vTaskDelay(pdMS_TO_TICKS(200));
um_buzzer_beep(3, 100, 50); // ...

// Мигание каждую секунду
while (1) {
    um_buzzer_toggle();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
```