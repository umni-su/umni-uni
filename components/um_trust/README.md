# UM Trust Component

Компонент доверительного управления (Trusted Management) для устройств UMNI на ESP-IDF.
Позволяет нескольким ESP32-устройствам объединяться в доверенную сеть (mesh): сопрягаться
по одноразовому коду, подписывать взаимные HTTP-запросы HMAC-SHA256 и удалённо управлять
возможностями (capabilities) друг друга с разграничением прав.

## Возможности

- ✅ Попарная (mesh) модель доверия между устройствами
- ✅ Сопряжение по одноразовому коду (PSK, base32, TTL 5 минут)
- ✅ HMAC-SHA256 подпись запросов + защита от replay (nonce) и от подмены времени (±60 с)
- ✅ Разграничение прав по capability (`adc`, `ntc`, `onewire`, `opentherm`, `in`, `out`, `oc`, `rf433`, `buzzer`)
- ✅ Определение IP peer'а по `device_id` через mDNS (`_umni_api._tcp`)
- ✅ Хранение конфигурации в `/spiffs/trust.json` с автосохранением
- ✅ Взаимный handshake при сопряжении (обе стороны добавляют друг друга)
- ✅ REST API для управления доверенными устройствами

## Зависимости

- ESP-IDF v5.5.2 или выше
- `cJSON`
- `um_nvs`, `um_storage`, `um_capabilities`, `um_helpers`, `um_sockets`
- `mbedtls` (HMAC-SHA256, constant-time compare)
- `esp_http_server`, `esp_http_client`, `esp_netif`

## Включение

Функция управляется опцией Kconfig `CONFIG_UM_FEATURE_TRUST` (по умолчанию `n`).
Для сборки `umni-c1` она включена в `configs/sdkconfig.defaults.umni-c1`:

```
CONFIG_UM_FEATURE_TRUST=y
```

Инициализация выполняется в `main.c` после `um_nvs_init()` и `um_storage_init()`:

```c
#if UM_FEATURE_ENABLED(TRUST)
    um_trust_init();
#endif
```

## Архитектура

```
                    mDNS (_umni_api._tcp)
   ┌──────────┐  <───────────────────>  ┌──────────┐
   │  Device  │                          │  Device  │
   │    A     │   HMAC-SHA256 подпись    │    B     │
   │ (owner)  │  <───────────────────>   │ (peer)   │
   └──────────┘   X-UMNI-Device/Ts/Nonce  └──────────┘
                       /Signature
```

- **Сопряжение**: устройство A генерирует одноразовый код `A:<base32-secret>`. Устройство B
  вводит код, сохраняет секрет и вызывает `A` на `/api/trust/confirm`, подписывая запрос этим
  секретом. A проверяет подпись по «висящему» секрету и добавляет B как доверенное устройство.
  После этого обе стороны имеют общий секрет.
- **Аутентификация**: запросы к `/api/*` принимаются либо с локальным токеном (админ), либо
  с подписью peer'а. Подпись покрывает метод, путь, timestamp, nonce и SHA256 тела.
- **Разграничение прав**: административные эндпоинты (`/api/configuration`, `/api/reboot`,
  `/api/ota`, `/api/trust`, `/api/settings`, `/api/automations`, `/api/login`) для peer'ов
  запрещены. Доступ к остальным определяется списком разрешений доверенного устройства.

## Схема подписи

Каноническая строка:

```
UMNI1
<METHOD>
<PATH>
<TIMESTAMP_MS>
<NONCE_HEX>
SHA256(body)_HEX
```

Заголовки запроса:

| Заголовок          | Описание                          |
|--------------------|-----------------------------------|
| `X-UMNI-Device`    | `device_id` отправителя           |
| `X-UMNI-Timestamp` | Unix-время в мс                   |
| `X-UMNI-Nonce`     | 16 случайных байт в hex           |
| `X-UMNI-Signature` | HMAC-SHA256 (hex) от канон. строки |

## HTTP API

| Метод  | Путь                          | Доступ | Описание                                   |
|--------|-------------------------------|--------|--------------------------------------------|
| GET    | `/api/trust/pairs`            | local  | Список доверенных устройств                |
| POST   | `/api/trust/pair/generate`    | local  | Сгенерировать одноразовый код сопряжения   |
| POST   | `/api/trust/pair/consume`     | local  | Ввести код и сопрячься с устройством       |
| POST   | `/api/trust/confirm`          | peer   | Подтверждение сопряжения (handshake)       |
| PUT    | `/api/trust/peer`             | local  | Обновить имя / права доверенного устройства|
| DELETE | `/api/trust/peer?id=<id>`     | local  | Удалить доверенное устройство              |
| POST   | `/api/trust/peer/state`       | local  | Запросить состояние capability у peer'а    |
| POST   | `/api/trust/peer/switch`      | local  | Переключить выход peer'а                   |

### Пример: сопряжение

На устройстве A (в веб-интерфейсе или через API):

```http
POST /api/trust/pair/generate
-> {"success":true,"data":{"pair":"umni-a:JBSWY3DPEHPK3PXP...","expires_in":300}}
```

На устройстве B:

```http
POST /api/trust/pair/consume
{"pair":"umni-a:JBSWY3DPEHPK3PXP..."}
-> {"success":true,"data":{"peer":"umni-a","confirmed":true}}
```

### Пример: управление peer'ом

```http
POST /api/trust/peer/state
{"id":"umni-a","capability":"temperature"}
```

## C API

```c
#include "um_trust.h"

// Инициализация
um_trust_init();

// Список доверенных
const um_trust_peer_t *peers[UM_TRUST_MAX_PEERS];
int n = um_trust_list(peers, UM_TRUST_MAX_PEERS);

// Проверка прав
bool can_control_outputs = um_trust_peer_has("umni-a", "out");

// Вызов peer'а (подписанный HTTP-запрос)
char *resp = NULL;
if (um_trust_call_peer("umni-a", "POST", "/api/state",
                       "{\"capability\":\"temperature\"}", &resp) == ESP_OK) {
    // ...
}
free(resp);

// Сопряжение
char pair[128];
um_trust_pair_generate(pair, sizeof(pair));

char peer_id[UM_TRUST_ID_MAX];
um_trust_pair_consume(pair, peer_id, sizeof(peer_id));
```

## Хранение

Конфигурация сохраняется в `/spiffs/trust.json` (версия формата — `UM_TRUST_CONFIG_VERSION`).
Запись выполняется фоновым таймером при изменениях; при выключении — через `um_trust_flush()`.

## Замечания по mbedTLS 4 (ESP-IDF v6.0)

В ESP-IDF v6.0 используется mbedTLS 4, где legacy API `mbedtls_md_hmac()` и
`mbedtls_ct_memcmp()` скрыты за макросом `MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS`
(такой же приём IDF применяет в своих port-обёртках `bignum.h`/`ecp.h`).
В `um_trust.c` этот макрос объявляется до подключения заголовков mbedTLS, а
constant-time сравнение берётся из `mbedtls/constant_time.h`.
