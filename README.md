# UMNI Uni — прошивка устройств UMNI

Единая прошивка на ESP-IDF для линейки устройств UMNI (ESP32, 4 МБ flash).
Один и тот же код собирается под разные устройства с помощью профилей в
[`configs/`](configs): `configs/sdkconfig.defaults.<устройство>`.

Профиль задаётся CMake-переменной `UMNI_DEVICE`, а состав функций и распиновка —
Kconfig-опциями `CONFIG_UM_FEATURE_*` / `CONFIG_UM_CFG_*`.

- Целевой чип: **esp32** (классический ESP32 / ESP32-WROVER, 4 МБ flash)
- ESP-IDF: **v6.0** (см. [Требования](#требования))
- Точка входа: [`main/main.c`](main/main.c), Kconfig: [`main/Kconfig.projbuild`](main/Kconfig.projbuild)
- Версия прошивки: `CONFIG_UMNI_FW_VERSION` (`sdkconfig.defaults`, сейчас `1.0.1`)

## Содержание

- [Требования](#требования)
- [Активация окружения ESP-IDF](#активация-окружения-esp-idf)
- [Как устроена конфигурация](#как-устроена-конфигурация)
- [Профили устройств](#профили-устройств)
- [Сборка](#сборка)
- [Прошивка и монитор](#прошивка-и-монитор)
- [Полезные команды idf.py](#полезные-команды-idfpy)
- [Разметка flash](#разметка-flash)
- [Добавление нового устройства](#добавление-нового-устройства)
- [Известные проблемы](#известные-проблемы)
- [Структура репозитория](#структура-репозитория)

---

## Требования

| Компонент | Требование |
| --- | --- |
| ESP-IDF | **v6.0 или новее** |
| Python | идёт в комплекте с ESP-IDF (venv), отдельно ставить не нужно |
| Чип | ESP32 (Xtensa), 4 МБ flash |
| Порт | USB-UART (CP210x/CH340), например `COM29` |
| Git | для клонирования и version info |

Почему именно v6.0: локальные компоненты
[`components/um_automations/idf_component.yml`](components/um_automations/idf_component.yml)
и [`components/um_network/idf_component.yml`](components/um_network/idf_component.yml)
требуют `idf >= 6.0`. С ESP-IDF 5.5.x сборка завершится ошибкой component manager.
Остальные компоненты формально допускают 5.5.2+, но проект в целом — 6.0+.

> Профили рассчитаны на ESP32 (ADC1-каналы 0/3/6/7, GPIO 0–39). Для другого чипа
> потребуется пересмотреть распиновку и ADC-настройки.

## Активация окружения ESP-IDF

Все команды `idf.py` выполняются в окружении ESP-IDF. Без него будет
`idf.py: command not found` или ошибка про Python venv.

### Windows — установка через ESP-IDF Installation Manager (EIM)

На этой машине используется установка EIM:

- ESP-IDF: `C:\Develop\ESP-IDF\v6.0\esp-idf`
- tools: `C:\Espressif\tools`
- Python venv: `C:\Espressif\tools\python\v6.0\venv`

Активация в новом окне PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -NoExit -Command "& 'C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1'"
```

Если политика выполнения уже разрешает локальные скрипты, достаточно:

```powershell
& 'C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1'
```

Проверка:

```powershell
idf.py --version    # ESP-IDF v6.0
echo $env:IDF_PATH  # C:\Develop\ESP-IDF\v6.0\esp-idf
```

> `C:\Develop\ESP-IDF\v5.5.2\esp-idf\export.bat` для этой машины не подходит:
> venv для 5.5.2 не содержит Python 3.13. Используйте EIM-профиль выше.

### Windows — классический установщик ESP-IDF

```bat
:: cmd.exe
C:\Espressif\frameworks\esp-idf-v6.0\export.bat
```

```powershell
# PowerShell
. C:\Espressif\frameworks\esp-idf-v6.0\export.ps1
```

### Linux / macOS

```bash
. $HOME/esp/esp-idf/export.sh
```

Проверка в любом окружении:

```bash
idf.py --version
```

## Как устроена конфигурация

```
sdkconfig.defaults                 общие настройки (версия, разделы, coredump, 4 МБ, все фичи = n)
        +
sdkconfig.defaults.esp32           общие настройки для ESP32 (I2C, Ethernet SPI, SD, OT, RF433,
        +                         1-Wire, PCF8574, ADC-каналы, индексы входов/выходов, automations)
configs/sdkconfig.defaults.<dev>   профиль конкретного устройства (включает нужные фичи/пины)
        =
sdkconfig                          сгенерированный итоговый конфиг (в git не хранится)
```

Корневой [`CMakeLists.txt`](CMakeLists.txt) делает две вещи:

1. `UMNI_DEVICE` — CMake-cache переменная (по умолчанию `umni-c1`). Из неё
   формируется макрос `UMNI_DEVICE="umni-c1"` для C-кода и путь к профилю:
   `configs/sdkconfig.defaults.${UMNI_DEVICE}`.
2. `SDKCONFIG_DEFAULTS` — список defaults-файлов в порядке применения:
   `sdkconfig.defaults` → `sdkconfig.defaults.esp32` (указан дважды, безвредно) →
   `configs/sdkconfig.defaults.${UMNI_DEVICE}`. Поздние файлы переопределяют ранние.

Файлы `configs/*` добавлены в include path, поэтому в коде доступен
`#include "base_config.h"` с макросами вида `UM_FEATURE_ENABLED(WEBHOOKS)`.

### Важное правило: defaults применяются только при создании `sdkconfig`

`sdkconfig` — генерируемый файл (в `.gitignore`). ESP-IDF читает
`SDKCONFIG_DEFAULTS` **только когда `sdkconfig` ещё не существует**. Если файл уже
есть, смена `UMNI_DEVICE` или правка профиля **не перезапишет** текущий
`sdkconfig` — сборка пройдёт со старым конфигом.

Поэтому при смене профиля сбрасывайте конфиг (см. [Сборка](#сборка)). Проверить,
какой профиль реально применён:

```powershell
Select-String -Path sdkconfig -Pattern '^CONFIG_UM_FEATURE_(ETHERNET|WIFI|OPENTHERM|TRUST)='
Select-String -Path build/CMakeCache.txt -Pattern '^UMNI_DEVICE:'
```

### `UMNI_DEVICE` vs `CONFIG_UM_FEATURE_*`

- `UMNI_DEVICE` — только выбор файла defaults и строка-идентификатор в коде
  (например, `um_mqtt_register_device(UMNI_DEVICE)`). Сам по себе он ничего не
  включает.
- Состав функций определяют `CONFIG_UM_FEATURE_*` из профиля.
- Распиновку и параметры задают `CONFIG_UM_CFG_*` (см. `main/Kconfig.projbuild`).

## Профили устройств

| Профиль | Файл | Состояние | Назначение |
| --- | --- | --- | --- |
| `umni-c1` | `configs/sdkconfig.defaults.umni-c1` | ✅ собирается | Полная конфигурация: Ethernet + Wi-Fi, 2 OC, buzzer, alarm, PCF8574 (6 входов / 8 выходов), OpenTherm, 1-Wire, 2×NTC, 2×AI, RF433, webserver, webhooks, trust |
| `umni-c1-mini` | `configs/sdkconfig.defaults.umni-c1-mini` | ✅ собирается, образ ~1,15 МБ | Урезанная C1: без OC, alarm и AI; 2 входа, 4 выхода; свой I2C/SPI/buzzer |
| `umni-ot` | `configs/sdkconfig.defaults.umni-ot` | ⚠️ пока не собирается | Файл пустой (0 байт) — все функции выключены; профиль-заготовка |
| `umni-skud` | — | ❌ нет профиля | Указан только в UI/CMake, файла defaults нет — сборка падает |

Состояние проверено на ESP-IDF v6.0. Причина ошибки `umni-ot` — в разделе
[Известные проблемы](#известные-проблемы). Команды сборки для всех профилей
одинаковые.

### Матрица функций

`y` — включено, `n`/`—` — выключено (для `umni-ot` файл пустой, поэтому всё `—`).

| Функция (Kconfig) | `umni-c1` | `umni-c1-mini` | `umni-ot` |
| --- | :---: | :---: | :---: |
| `UM_FEATURE_ETHERNET` | y | y | — |
| `UM_FEATURE_WIFI` | y | y | — |
| `UM_FEATURE_MQTT` | n | n | — |
| `UM_FEATURE_WEBSERVER` | y | y | — |
| `UM_FEATURE_WEBHOOKS` | y | y | — |
| `UM_FEATURE_TRUST` | y | y | — |
| `UM_FEATURE_OPENTHERM` | y | y | — |
| `UM_FEATURE_ONEWIRE` | y | y | — |
| `UM_FEATURE_RF433` | y | y | — |
| `UM_FEATURE_OPENCOLLECTORS` | y | n | — |
| `UM_FEATURE_OC1` / `OC2` | y / y | n / n | — |
| `UM_FEATURE_BUZZER` | y | y | — |
| `UM_FEATURE_ALARM` | y | n | — |
| `UM_FEATURE_INPUTS` | y | y | — |
| `UM_FEATURE_INP1`…`INP6` | y×6 | y, y, n, n, n, n | — |
| `UM_FEATURE_OUTPUTS` | y | y | — |
| `UM_FEATURE_OUT1`…`OUT8` | y×8 | y, y, y, y, n, n, n, n | — |
| `UM_FEATURE_NTC` / `NTC1` / `NTC2` | y / y / y | y / y / y | — |
| `UM_FEATURE_AI` / `AI1` / `AI2` | y / y / y | n / n / n | — |
| `UM_FEATURE_SDCARD` | n | n | — |
| `UM_FEATURE_AUTOMATIONS` | y | y | y* |

\* `UM_FEATURE_AUTOMATIONS=y` задаётся в общем `sdkconfig.defaults.esp32`, поэтому
включается для всех профилей, включая `umni-ot`.

### Распиновка профилей

| Параметр | `umni-c1` | `umni-c1-mini` |
| --- | --- | --- |
| I2C SDA / SCL (PCF8574) | 23 / 22 | 21 / 22 |
| Ethernet SPI host | 1 | 1 |
| Ethernet SCLK / MOSI / MISO / CS / INT | 18 / 21 / 19 / 5 / −1 | 18 / 23 / 19 / 5 / −1 |
| SD SPI (выключен) host / SCLK / MOSI / MISO / CS / DET | 1 / 18 / 21 / 19 / 32 / 33 | 1 / 18 / 21 / 19 / 32 / 33 |
| OpenTherm IN / OUT | 26 / 25 | 26 / 25 |
| 1-Wire | 17 | 17 |
| RF433 data | 27 | 27 |
| Alarm | 13 | 13 (фича выкл.) |
| Open Collector 1 / 2 | 14 / 12 | 14 / 12 (фича выкл.) |
| Buzzer | 15 | 4 |
| PCF8574 INT | 16 | 16 |
| NTC1 / NTC2 ADC-канал (GPIO) | 0 (36) / 3 (39) | 0 (36) / 3 (39) |
| AI1 / AI2 ADC-канал (GPIO) | 6 (34) / 7 (35) | выкл. |
| Индексы INP1…INP6 на PCF8574 | 2, 3, 1, 0, 5, 4 | 2, 3 (остальные выкл.) |
| Индексы OUT1…OUT8 на PCF8574 | 0, 1, 2, 3, 5, 4, 6, 7 | 0, 1, 2, 3 (остальные выкл.) |

Пины выключенных функций остаются в профиле, но не используются.

## Сборка

Все `-D`/`-B` — **глобальные** опции `idf.py`, их нужно ставить **до** имени
действия (`build`, `flash`, …).

### Обычная сборка текущего профиля

```powershell
idf.py build
```

Использует уже существующий `sdkconfig` и закешированный `UMNI_DEVICE` из
`build/CMakeCache.txt` (при первом запуске — `umni-c1`).

Готовый образ: `build/umni-uni.bin`.

### Сборка/переключение на конкретное устройство

Правильный способ сбросить конфиг и применить профиль — `set-target` (он
переименует `sdkconfig` в `sdkconfig.old` и очистит `build/`):

```powershell
idf.py -D UMNI_DEVICE=umni-c1 set-target esp32
idf.py build
```

```powershell
idf.py -D UMNI_DEVICE=umni-c1-mini set-target esp32
idf.py build
```

```powershell
idf.py -D UMNI_DEVICE=umni-ot set-target esp32
idf.py build
```

> `umni-skud` сейчас собрать нельзя: нет файла
> `configs/sdkconfig.defaults.umni-skud`, CMake завершится с
> `SDKCONFIG_DEFAULTS ... does not exist`.

Эквивалент без `set-target` (вручную):

```powershell
Remove-Item sdkconfig -ErrorAction SilentlyContinue
idf.py -D UMNI_DEVICE=umni-c1-mini fullclean build
```

```bash
rm -f sdkconfig && idf.py -D UMNI_DEVICE=umni-c1-mini fullclean build
```

> Просто `idf.py -D UMNI_DEVICE=umni-c1-mini build` при существующем `sdkconfig`
> **не применит** профиль mini: конфиг останется от предыдущего устройства.

### Проверка применённого профиля

```powershell
Select-String -Path sdkconfig -Pattern '^CONFIG_UM_FEATURE_'
Select-String -Path build/CMakeCache.txt -Pattern '^UMNI_DEVICE:|^SDKCONFIG_DEFAULTS:'
idf.py size
```

### Несколько профилей параллельно (CI / несколько плат)

Чтобы не пересоздавать общий `sdkconfig`, каждому профилю можно дать свой файл
конфига и свой build-каталог:

```powershell
$root = (Get-Location).Path

idf.py -B build-c1    -D SDKCONFIG="$root/sdkconfig.c1"    -D UMNI_DEVICE=umni-c1      build
idf.py -B build-mini  -D SDKCONFIG="$root/sdkconfig.mini"  -D UMNI_DEVICE=umni-c1-mini build
```

- `SDKCONFIG` должен указывать на **несуществующий** файл при первой сборке —
  тогда применятся defaults профиля.
- `sdkconfig.c1` / `sdkconfig.mini` не покрыты `.gitignore` (там только
  `sdkconfig` и `sdkconfig.old`) — не коммитьте их.
- `dependencies.lock` и `managed_components/` общие для всех сборок, поэтому
  параллельный запуск двух `idf.py` в одном дереве лучше не делать.
- Для повторной сборки достаточно `idf.py -B build-c1 build` (значения `-D`
  кешируются в `build-c1/CMakeCache.txt`).

### menuconfig

```powershell
idf.py menuconfig
```

Раздел **UMNI Configuration** содержит все `UM_FEATURE_*` и `UM_CFG_*`.
Изменения попадают в `sdkconfig` (локальный, в git не хранится). Чтобы они
сохранились для команды, перенесите нужные строки в
`configs/sdkconfig.defaults.<устройство>` (или в общие defaults, если настройка
общая для всех устройств) и пересоздайте `sdkconfig` через `set-target`.

### Изменение версии прошивки

```ini
# sdkconfig.defaults
CONFIG_UMNI_FW_VERSION="1.0.1"
```

После правки нужен новый `sdkconfig` (`set-target`) или ручная правка
`CONFIG_UMNI_FW_VERSION` в текущем `sdkconfig`.

## Прошивка и монитор

Порт задаётся флагом `-p` (или переменной `ESPPORT`). Узнать порт можно в
диспетчере устройств Windows (`COMx`).

```powershell
# прошить и сразу открыть монитор
idf.py -p COM29 flash monitor

# только прошить
idf.py -p COM29 flash

# только монитор (выход — Ctrl+])
idf.py -p COM29 monitor

# прошить только приложение (bootloader и таблица разделов не трогаются)
idf.py -p COM29 app-flash
```

Полное стирание flash (удаляет NVS, SPIFFS-конфиги, OTA-данные — используйте
осознанно):

```powershell
idf.py -p COM29 erase-flash
```

Сброс только OTA-метаданных (если плата зациклилась на неудачном OTA):

```powershell
idf.py -p COM29 erase-otadata
```

Скорость/режим flash заданы в образе (`dio`, 40 МГц, 4 МБ) и применяются
автоматически.

### Ручная прошивка через esptool (без idf.py)

Команда соответствует сгенерированному `build/flash_args`:

```powershell
python -m esptool --chip esp32 -b 460800 --before default-reset --after hard-reset write-flash `
  --flash-mode dio --flash-size 4MB --flash-freq 40m `
  0x1000 build\bootloader\bootloader.bin `
  0x8000 build\partition_table\partition-table.bin `
  0x10000 build\ota_data_initial.bin `
  0x50000 build\umni-uni.bin
```

Или из каталога `build` (там пути в `flash_args` относительные):

```powershell
Set-Location build
esptool.py --chip esp32 -p COM29 -b 460800 write_flash "@flash_args"
```

### OTA

В прошивке две OTA-секции (`ota_0`, `ota_1`), образ обновляется через
`POST /api/ota/update` webserver-компонента (`um_webserver`). Авторизация —
как у остальных `/api/*` (Bearer-токен / trust HMAC, см.
`components/um_trust/README.md`). После неудачного OTA можно вернуться командой
`idf.py -p COM29 erase-otadata`.

### Отладка

```powershell
idf.py openocd          # запустить OpenOCD (плата ESP32-WROVER-KIT)
idf.py gdb              # подключить GDB
idf.py gdbgui           # GDB с веб-интерфейсом
idf.py gdbtui           # GDB TUI
```

Coredump включён (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`, раздел `coredump`):

```powershell
idf.py -p COM29 coredump-info    # стек и регистры упавшей задачи
idf.py -p COM29 coredump-debug   # собрать ELF и открыть GDB
```

## Полезные команды idf.py

| Команда | Что делает |
| --- | --- |
| `idf.py --version` | версия ESP-IDF |
| `idf.py build` | инкрементальная сборка |
| `idf.py fullclean` | удалить `build/` (sdkconfig **остаётся**) |
| `idf.py clean` | удалить артефакты сборки, сохранить конфигурацию сборки |
| `idf.py reconfigure` | перегенерировать CMake/Kconfig (после правки CMakeLists/Kconfig) |
| `idf.py set-target esp32` | сменить target, переименовать `sdkconfig` → `sdkconfig.old`, очистить build |
| `idf.py menuconfig` | конфигуратор Kconfig |
| `idf.py size` | размер образа и секций |
| `idf.py size-components` | размер по компонентам |
| `idf.py size-files` | размер по файлам |
| `idf.py app` | собрать только приложение |
| `idf.py bootloader` | собрать только bootloader |
| `idf.py partition-table` | собрать только таблицу разделов |
| `idf.py -p COMx flash` | прошить проект |
| `idf.py -p COMx app-flash` | прошить только приложение |
| `idf.py -p COMx bootloader-flash` | прошить только bootloader |
| `idf.py -p COMx partition-table-flash` | прошить только таблицу разделов |
| `idf.py -p COMx monitor` | монитор UART |
| `idf.py -p COMx flash monitor` | прошить и открыть монитор |
| `idf.py -p COMx erase-flash` | стереть весь flash |
| `idf.py -p COMx erase-otadata` | стереть OTA-метаданные |
| `idf.py -p COMx read-otadata` | прочитать OTA-метаданные |
| `idf.py -p COMx coredump-info` | разобрать coredump |
| `idf.py -p COMx coredump-debug` | coredump + GDB |
| `idf.py openocd` / `gdb` / `gdbgui` / `gdbtui` | отладка через JTAG |
| `idf.py -v build` | подробный вывод сборки |

## Разметка flash

Используется [`partitions-coredump.csv`](partitions-coredump.csv)
(`CONFIG_PARTITION_TABLE_CUSTOM_FILENAME`), 4 МБ:

| Offset | Раздел | Размер | Назначение |
| --- | --- | --- | --- |
| `0x1000` | bootloader | — | загрузчик |
| `0x8000` | partition table | 4 КБ | таблица разделов |
| `0x9000` | `nvs` | 24 КБ | настройки (Wi-Fi, токены, конфиги) |
| `0xF000` | `phy_init` | 4 КБ | калибровка RF |
| `0x10000` | `otadata` | 8 КБ | выбор OTA-слота |
| `0x12000` | `storage` (SPIFFS) | 184 КБ | JSON-конфиги (`/spiffs`) |
| `0x40000` | `coredump` | 64 КБ | дамп падений |
| `0x50000` | `ota_0` | 1,5 МБ | прошивка №1 |
| `0x1D0000` | `ota_1` | 1,5 МБ | прошивка №2 |

Текущий образ C1 занимает ~1,16 МБ (`idf.py size`), т.е. до лимита OTA-слота
(~1,5 МБ) остаётся ~0,34 МБ. Учитывайте это при добавлении зависимостей.

`partitions.csv` — устаревший вариант без coredump, в сборке **не используется**.

> При изменении таблицы разделов нужно перепрошить таблицу и стереть flash:
> `idf.py -p COMx erase-flash flash` (сместятся данные NVS/SPIFFS).

## Добавление нового устройства

1. Создайте профиль из существующего:

   ```powershell
   Copy-Item configs\sdkconfig.defaults.umni-c1 configs\sdkconfig.defaults.umni-new
   ```

2. Отредактируйте `CONFIG_UM_FEATURE_*` и `CONFIG_UM_CFG_*`. Полный список
   опций — в [`main/Kconfig.projbuild`](main/Kconfig.projbuild). Файл должен
   существовать даже если он пустой (как `umni-ot`), иначе CMake упадёт.

3. (Необязательно) добавьте имя в подсказки CMake и VS Code:

   ```cmake
   set_property(CACHE UMNI_DEVICE PROPERTY STRINGS umni-c1 umni-c1-mini umni-ot umni-skud)
   ```

   ```jsonc
   // .vscode/tasks.json (локальный, в git не хранится)
   "options": ["umni-c1", "umni-c1-mini", "umni-ot", "umni-skud"]
   ```

   На саму сборку это не влияет: `UMNI_DEVICE` принимает любое имя.

4. Соберите и проверьте именно этот профиль:

   ```powershell
   idf.py -D UMNI_DEVICE=umni-new set-target esp32
   idf.py build
   idf.py size
   ```

5. Обновите таблицы профилей/распиновки в этом README и в `AGENTS.md`.

Если нужна новая опция (пин, функция), добавьте её в `main/Kconfig.projbuild`:

```kconfig
config UM_CFG_MY_GPIO
    int "My GPIO"
    range 0 39
    default 21
    help
        GPIO for my feature
```

и используйте в коде через `CONFIG_UM_CFG_MY_GPIO`. Для булевой функции —
`config UM_FEATURE_MY`, значение по умолчанию `n`, и профиль должен явно
включать её (`CONFIG_UM_FEATURE_MY=y`).

## Известные проблемы

Проверено на ESP-IDF v6.0 (12.11.2026).

| # | Проблема | Причина / решение |
| --- | --- | --- |
| 1 | После `idf.py -D UMNI_DEVICE=... build` профиль не меняется | `sdkconfig` уже существует, defaults не применяются. Используйте `set-target` или удалите `sdkconfig` |
| 2 | `umni-ot` не собирается | Пустой профиль (все фичи `n`): `um_dio.c`/`um_dio.h` требуют `CONFIG_UM_CFG_INPn_INDEX`/`OUTn_INDEX`, которых нет при выключенных `UM_FEATURE_INPUTS/OUTPUTS` |
| 3 | При выключенной фиче её API приходится закрывать `#if` в каждом потребителе | Публичные заголовки (`um_opencollectors.h`, `um_adc.h`) объявляют типы под `#if CONFIG_UM_FEATURE_*`, а `.c`-файлы и другие компоненты (`um_automations`, `um_webserver`) собираются всегда. Пример исправления — `#if UM_FEATURE_ENABLED(OC1)` вокруг вызовов OC-API |
| 4 | `umni-skud` не собирается | Нет `configs/sdkconfig.defaults.umni-skud`: `SDKCONFIG_DEFAULTS ... does not exist` |
| 5 | Сборка всех компонентов занимает одинаковое время независимо от фич | В `CMakeLists.txt` фильтрация по `COMPONENTS_BASE_DIR`/`UM_FEATURE_*` не работает: `COMPONENTS_BASE_DIR` нигде не задан, а `UM_FEATURE_*` ещё не загружены до `include(project.cmake)`. Все `components/um_*` собираются всегда; фичи отключаются только в C-коде через `#if` |
| 6 | На Linux/macOS/CI компонент «пропадает» | В 5 компонентах файл называется `CmakeLists.txt` (строчная `m`): `um_alarm`, `um_automations`, `um_ethernet`, `um_opencollectors`, `um_opentherm`. На Windows работает, на регистрозависимой ФС — нет. Переименуйте в `CMakeLists.txt` |
| 7 | `dependencies.lock` изменился после сборки в новом каталоге | Манифесты используют `version: "*"`, component manager может подтянуть более новые версии. Проверяйте `git diff dependencies.lock` и коммитьте осознанно |
| 8 | В `.vscode/settings.json` указан IDF v5.5.2, а сборка идёт v6.0 | Локальный файл (в git не хранится). Приведите `idf.espIdfPathWin`/`idf.currentSetup` к v6.0 |
| 9 | `CONFIG_UM_CFG_AI*_ADC_CHANNEL` / `NTC*_ADC_CHANNEL` не меняется | Это вычисляемые `int`-символы от `choice`. Меняйте choice-символ, например `CONFIG_UM_CFG_AI1_ADC_CHANNEL_7=y` |
| 10 | `AGENTS.md` не виден в git | Он в `.gitignore` (строка `AGENTS.md`). Уберите строку, если файл нужно версионировать |

## Структура репозитория

```
CMakeLists.txt                 выбор UMNI_DEVICE + SDKCONFIG_DEFAULTS
sdkconfig.defaults             общие настройки
sdkconfig.defaults.esp32       общие настройки для ESP32
configs/
  base_config.h                макросы UM_FEATURE_ENABLED()/IF_FEATURE_ENABLED()
  sdkconfig.defaults.<dev>     профили устройств
main/
  main.c                       app_main(), инициализация и задачи
  Kconfig.projbuild            все UMNI-опции (UMNI Configuration)
partitions-coredump.csv        используемая таблица разделов (4 МБ)
partitions.csv                 устаревшая, не используется
components/um_*                модули прошивки (см. их README.md)
managed_components/            скачанные зависимости (в git не хранятся)
dependencies.lock              зафиксированные версии зависимостей
build/                         артефакты сборки (в git не хранятся)
```

### Компоненты

| Компонент | Роль |
| --- | --- |
| `um_events` | шина событий ESP-IDF (`UMNI_EVENT_*`) |
| `um_capabilities` | реестр включённых возможностей устройства |
| `um_nvs`, `um_storage`, `um_store` | NVS, SPIFFS (`/spiffs`), JSON-хранилище |
| `um_network`, `um_wifi`, `um_ethernet` | сеть: Wi-Fi и W5500 (SPI) |
| `um_sockets`, `um_sse_server`, `um_webserver` | TCP-сокеты, SSE, REST API + OTA |
| `um_webhooks`, `um_trust`, `um_mqtt` | вебхуки, доверенное управление (HMAC), MQTT |
| `um_dio`, `um_opencollectors`, `um_buzzer`, `um_alarm` | дискретные входы/выходы PCF8574, OC, buzzer, alarm |
| `um_adc_common`, `um_adc`, `um_ntc` | аналоговые входы и NTC |
| `um_onewire`, `um_opentherm`, `um_rf433` | 1-Wire (DS18x20), OpenTherm, RF433 |
| `um_sd`, `um_automations` | microSD (SPI), автоматизации |

Подробности по API — в `README.md` соответствующего компонента.

## Лицензия

См. манифесты компонентов (`license: MIT`).
