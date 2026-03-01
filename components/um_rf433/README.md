# Управление конфигурацией RF433 датчиков

## Архитектура хранения
### 1. Постоянное хранилище (JSON)
Путь: `/spiffs/rf433.json`
```json
json
{
    "devices": [
        {
            "serial": 123456,
            "type": 1,
            "label": "Входная дверь",
            "alarm": true
        }
    ]
}
```
### 2. Runtime массив
Массив: `rf_devices[UM_RF433_MAX_SENSORS]`

```c
typedef struct {
    uint32_t serial;
    long time;
    long last_processed_time;
    bool alarm;
    bool triggered;
    uint8_t state;
    uint8_t packet_count;
} um_rf433_device_t;
```
### Принцип работы
- Загрузка конфигурации
`um_rf433_config_load()`
- Чтение JSON из SPIFFS
- Создание записей в rf_devices[]
- Копирование serial и alarm в RAM
- Поля type и label остаются только в JSON
- Обработка сигналов
- Получение serial от датчика
- Поиск в rf_devices[] по serial
- При найденном датчике:
- Обновление time, state, packet_count
- Анализ triggered
- Проверка alarm
- При ненайденном датчике:
   - Игнорирование сигнала
   - Логирование в режиме поиска

### API управления
```c
// Добавление датчика
esp_err_t um_rf433_config_add_device(
    uint32_t serial,
    uint8_t type,
    const char *label,
    bool alarm
);

// Обновление датчика
esp_err_t um_rf433_config_update_device(
    uint32_t serial,
    uint8_t type,
    const char *label,
    bool alarm
);

// Удаление датчика
esp_err_t um_rf433_config_remove_device(uint32_t serial);
```
### Синхронизация
- При вызове API обновляется rf_devices[]
- Автоматический вызов um_rf433_config_save()
- Запись изменений в JSON файл

### Режим поиска
```c
// Активация
um_rf433_activale_search();

// Массив обнаруженных датчиков
rf_scanned_devices[UM_RF433_MAX_SEARCH_SENSORS]

// Деактивация
um_rf433_clear_search();
```
### Поток данных
```text
[RF сигнал] → [Приемник] → [rf433_receiver_task] → [Поиск в rf_devices[]] 
    ↓
[Найден] → Обновление runtime → [triggered] → Событие
    ↓
[Не найден] → [Режим поиска] → Сохранение в rf_scanned_devices[]
```
### Особенности
- Ручное добавление: Датчики добавляются только через API
- Экономия RAM: В памяти только данные для реального времени
- Автосинхронизация: rf_devices[] всегда соответствует конфигу
- Быстрый доступ: Критичные данные (alarm) дублируются в RAM

### Пример
```c
// Инициализация
um_rf433_config_load();

// Поиск новых датчиков
um_rf433_activale_search();

// Добавление обнаруженного датчика
um_rf433_config_add_device(0x123456, 1, "Гараж", true);

// Обновление
um_rf433_config_update_device(0x123456, 1, "Ворота", false);

// Удаление
um_rf433_config_remove_device(0x123456);
```
### Заключение
Конфиг управляет runtime массивом. Изменения вносятся через API, автоматически сохраняются в JSON и синхронизируются с rf_devices[] для обработки в реальном времени.

