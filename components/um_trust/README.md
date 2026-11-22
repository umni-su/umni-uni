# UM Trust — компонент доверительного управления (mesh)

Компонент доверительного управления для устройств UMNI на ESP-IDF. Позволяет нескольким ESP32 объединяться в доверенную сеть: сопряжаться по одноразовому коду, подписывать взаимные HTTP-запросы HMAC-SHA256 и удалённо управлять возможностями (capabilities) друг друга с разграничением прав.

## Возможности

- ✅ Попарная (mesh) модель доверия между устройствами
- ✅ Сопряжение по одноразовому коду (PSK, base32, TTL 5 минут)
- ✅ HMAC-SHA256 подпись запросов + защита от replay (nonce) и от подмены времени (±60 с)
- ✅ Разграничение прав по capability (`adc`, `ntc`, `onewire`, `opentherm`, `in`, `out`, `oc`, `rf433`, `buzzer`)
- ✅ Определение IP peer'а по `device_id` через mDNS (`_umni_api._tcp`) с кэшем адреса
- ✅ Хранение конфигурации в `/spiffs/trust.json` с автосохранением
- ✅ Взаимный handshake при сопряжении (обе стороны добавляют друг друга)
- ✅ REST API для управления доверенными устройствами и правами

## Зависимости

- ESP-IDF v5.5.2+ (проверено на v6.0.0 / mbedTLS 4)
- `cJSON`, `um_nvs`, `um_storage`, `um_capabilities`, `um_helpers`, `um_sockets`
- `mbedtls` (HMAC-SHA256, constant-time compare)
- `esp_http_server`, `esp_http_client`, `esp_netif`

## Включение

Функция управляется опцией Kconfig `CONFIG_UM_FEATURE_TRUST` (по умолчанию `n`). Для `umni-c1` она включена в `configs/sdkconfig.defaults.umni-c1`:

```
CONFIG_UM_FEATURE_TRUST=y
```

Инициализация в `main.c` **после** `um_nvs_init()` и `um_storage_init()`:

```c
#if UM_FEATURE_ENABLED(TRUST)
    um_trust_init();
#endif
```

## Модели авторизации

Каждый HTTP-запрос к `/api/*` попадает под одну из трёх моделей:

| Режим               | Константа       | Как получается                                                                                                                     | Права                                      |
| ------------------- | --------------- | ---------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------ |
| **Нет авторизации** | `UM_AUTH_NONE`  | Конфигурационный режим (перемычка) или токен не задан                                                                              | Ограничено (см. `check_auth`)              |
| **Локальный админ** | `UM_AUTH_LOCAL` | Bearer-токен (`Authorization: Bearer <token>` / `?token=`) **или** Admin-HMAC (`X-UMNI-Device: admin` + подпись вебсервер-токеном) | Полный доступ ко всем эндпоинтам           |
| **Peer-устройство** | `UM_AUTH_PEER`  | Подпись HMAC-SHA256 общим секретомpeer-а (`X-UMNI-Device: <device_id>`)                                                            | Права проверяются через `um_trust_enforce` |

Admin-HMAC использует вебсервер-токен из NVS (ключ `httptoken`) как HMAC-ключ. Подробности — в `components/um_trust/HMAC_ADMIN.md`.

## Архитектура

```
                    mDNS (_umni_api._tcp)
   ┌─────────────┐   ───────────────────   ┌─────────────┐
   │ Device A    │   HMAC-SHA256           │ Device B    │
   │ (владелец)  │   X-UMNI-Device/Ts/     │ (peer)      │
   └─────────────┐   Nonce / Signature     └─────────────┘
                  └──────────────────────→┘
```

## Схема подписи (peer и admin-HMAC)

Каноническая строка (каждое поле с новой строки):

```
UMNI1
<METHOD>
<PATH>
<TIMESTAMP_MS>
<NONCE_HEX>
SHA256(body)_HEX
```

Заголовки запроса:

| Заголовок            | Значение                                                                                                     |
| -------------------- | ------------------------------------------------------------------------------------------------------------ |
| `X-UMNI-Device`      | `device_id` отправителя (для админа — зарезервировано `admin`)                                               |
| `X-UMNI-Timestamp`   | Unix-время в мс (проверка окна ±60 000 мс, требуется NTP)                                                    |
| `X-UMNI-Nonce`       | 16 случайных байт в hex (32 символа); повтор запрещён (replay-кольцо на 64 записи)                           |
| `X-UMNI-Signature`   | HMAC-SHA256 (hex, 64 симв.) от канонической строки ключом = secret peer-а (или вебсервер-токеном для админа) |
| `X-UMNI-Body-SHA256` | только для OTA — хэш прошивки, подписывается вместо тела                                                     |

## Сопряжение (pairing) — полный цикл

Устройства **A и B добавляют друг друга одновременно**. После успешного сопряжения у обоих в trust.json появляется запись о второй стороне, но **с пустым списком прав** — права выдаются отдельным шагом (см. раздел «Права»).

### Последовательность

```
Устройство A                          Устройство B
   │                                       │
   ├─ 1. POST /api/trust/pair/generate     │
   │   (Bearer/админ-HMAC, локально)       │
   │   └─ создаёт одноразовый секрет       │
   │   └─ возвращает "A:<base32>", TTL 5 мин│
   │                                       │
   │        pair-код "A:<base32>"           │
   │ ─────────────────────────────────────►│
   │                                       │
   │                                       ├─ 2. POST /api/trust/pair/consume
   │                                       │   (Bearer/админ-HMAC, локально на B)
   │                                       │   └─ декодирует id=A + секрет
   │                                       │   └─ сохраняет peer A (права = пусто)
   │                                       │   └─ вызывает A: POST /api/trust/confirm
   │                                       │       (подпись секретом A)
   │                                       │
   │         ◄─────────────────────────────┤
   │              confirm                  │
   │        POST /api/trust/confirm        │
   │   (A принимает «висящий» секрет B)    │
   │   └─ проверяет HMAC по слотам пар     │
   │   └─ сохраняет peer B (права = пусто) │
   │   └─ ответ {peer:"B", paired:true}    │
   │                                       │
   └─ обе стороны имеют共享 секрет и запись друг о друге
```

### Подробно по шагам

**Шаг 1 — генерация кода (устройство A, локальный админ):**

```http
POST /api/trust/pair/generate
Authorization: Bearer <webserver-токен A>

-> {"success":true,"data":{"pair":"umni-a:JBSWY3DPEHPK3PXP...","expires_in":300}}
```

Код формируется как `<device_id_A>:<base32(32 байта)>` и живёт 5 минут. До 4 кодов одновременно.

**Шаг 2 — приём кода и взаимный handshake (устройство B, локальный админ):**

```http
POST /api/trust/pair/consume
Authorization: Bearer <webserver-токен B>
Content-Type: application/json

{"pair":"umni-a:JBSWY3DPEHPK3PXP..."}

-> {"success":true,"data":{"peer":"umni-a","confirmed":true}}
```

B сохраняет A как peer с общим секретом, затем сам вызывает `A: POST /api/trust/confirm` с подписью этим секретом. Если подтверждение прошло, обе стороны добавляют друг друга. Поле `confirmed` отражает, подтвердила ли сторона A handshake.

**Шаг 3 — результат:** у A есть запись о B, у B — запись о A. Список прав у обоих пуст `[]`. Пока права не выданы, peer не может вызвать ни одного управляющего эндпоинта (кроме `/api/systeminfo`, который открыт для всех peers).

## Права (разграничение доступа)

### Кто выдаёт права

Права выдаёт **локальный администратор устройства-владельца ресурса** через `PUT /api/trust/peer` на том устройстве, к которому хочет получить доступ другой.

**Нет автоматической выдачи прав при сопряжении.** Permissions пусты. Устройство B не может само выдать себе права на A — только администратор A может разрешить B доступ к своим возможностям.

### Набор прав (capability)

| Право (строка) | Что разрешает peer-устройству                                                                             |
| -------------- | --------------------------------------------------------------------------------------------------------- |
| `out`          | Управлять цифровыми выходами: `POST /api/switch` (`mode:outputs`), `POST /api/state` (`capability: outN`) |
| `oc`           | Управлять открытыми коллекторами: `POST /api/switch` (`mode:opencollectors`)                              |
| `in`           | Читать цифровые входы: `GET /api/conf?section=inputs`                                                     |
| `adc`          | Читать АЦП: `GET /api/conf?section=adc` или `?section=ai`                                                 |
| `ntc`          | Читать NTC-датчики: `GET /api/conf?section=ntc`                                                           |
| `onewire`      | Читать 1-Wire: `GET /api/conf?section=onewire`                                                            |
| `rf433`        | Сканировать RF433: `POST /api/rf/scan`                                                                    |
| `buzzer`       | Гудить:`POST /api/beep`                                                                                   |
| `opentherm`    | (зарезервировано, пока не применяется в `um_trust_enforce`)                                               |

### Как выдаются права (устройство A разрешает B)

```http
PUT /api/trust/peer
Authorization: Bearer <токен A>
Content-Type: application/json

{
  "id": "umni-b",
  "name": "Котельная B",
  "permissions": ["out", "adc", "ntc"]
}

-> {"success":true,"data":{"id":"umni-b","name":"Котельная B","permissions":["out","adc","ntc"],...}}
```

После этого устройство B, подписывая запрос своим shared-секретом, может вызывать на A только те эндпоинты, которые покрываются правами `out`, `adc`, `ntc`.

Права хранятся в массиве `permissions` peer-а (макс. `UM_TRUST_PERMS_MAX = 16` строк, каждая до `UM_TRUST_PERM_LEN_MAX = 24` символов).

## Управление состоянием и переключение (state / switch)

Оба эндпоинта вызываются **локальным админом устройства-инициатора**, который действует от имени доверенного peer-а. Устройство-инициатор подписанно своим shared-секретом к peer-у.

### Запрос состояния capability у peer

```http
POST /api/trust/peer/state
Authorization: Bearer <токен инициатора>
Content-Type: application/json

{
  "id": "umni-b",
  "capability": "ntc"
}
```

Внутри устройство-инициатор вызывает `peer: POST /api/state {"capability":"ntc"}` с подписью. Peer проверяет `um_trust_peer_has(инициатор, "ntc")` — если права есть, возвращает состояние.

### Переключение выхода у peer

```http
POST /api/trust/peer/switch
Authorization: Bearer <токен инициатора>
Content-Type: application/json

{
  "id": "umni-b",
  "mode": "outputs",
  "index": 1,
  "level": true
}
```

Внутри вызывается `peer: POST /api/switch {"mode":"outputs","index":1,"level":true}`. Peer проверяет право `out` (для `mode:outputs`) или `oc` (для `mode:opencollectors`).

`index` — 1-based (в коде `um_dio_set_output(idx-1, lvl)`).

## Удаление trusted device (unpair)

```http
DELETE /api/trust/peer?id=umni-b
Authorization: Bearer <токен устройства-владельца>

-> {"success":true,"data":{"id":"umni-b"}}
```

Удаляет запись о peer **только с этой стороны** (односторонне). Чтобы полностью разорвать доверие, удаление нужно выполнить **на обоих устройствах**: на A удалить B, на B удалить A. Автоматического уведомления peer-а об удалении нет. После удаления старый shared-секрет стирается, подписанные запросы от удалённого устройства перестают проходить `um_trust_verify_body` ( peer не найден → `ESP_ERR_NOT_FOUND`).

## Все REST API um_trust

| Метод  | Путь                       | Доступ               | Тело запроса                         | Описание                                                                                     |
| ------ | -------------------------- | -------------------- | ------------------------------------ | -------------------------------------------------------------------------------------------- |
| GET    | `/api/trust/pairs`         | **local**            | —                                    | Список всех доверенных устройств с их правами, именами, IP, датами                           |
| POST   | `/api/trust/pair/generate` | **local**            | —                                    | Сгенерировать одноразовый код сопряжения (`pair`, `expires_in=300`)                          |
| POST   | `/api/trust/pair/consume`  | **local**            | `{"pair":"A:<base32>"}`              | Принять код, сохранить peer, инициировать взаимный confirm                                   |
| POST   | `/api/trust/confirm`       | **peer** (handshake) | —                                    | Принять «висящий» секрет от устройства, зарегистрировав его как peer (только в ходе pairing) |
| PUT    | `/api/trust/peer`          | **local**            | `{"id","name?","permissions":[...]}` | Обновить имя и/или права доверенного устройства                                              |
| DELETE | `/api/trust/peer?id=<id>`  | **local**            | —                                    | Удалить доверенное устройство                                                                |
| POST   | `/api/trust/peer/state`    | **local**            | `{"id","capability"}`                | Запросить состояние capability у peer (вызов peer: `/api/state`)                             |
| POST   | `/api/trust/peer/switch`   | **local**            | `{"id","mode","index","level"}`      | Переключить выход/OC у peer (вызов peer: `/api/switch`)                                      |

> **Важно:** все эндпоинты `/api/trust/*` кроме `/api/trust/confirm` доступны **только локальному админу** (Bearer или Admin-HMAC). Peer-устройство не может спискать пары, менять права или удалять устройства — вернёт `403 Forbidden`. `/api/trust/confirm` Special-case: вызывается только внутри handshake пары, enforce не применяется.

Примеры ответов:

```
GET /api/trust/pairs
-> {"success":true,"data":[
  {"id":"umni-b","name":"Котельная B","permissions":["out","adc"],"last_ip":"192.168.1.42","last_seen_ms":1719000000000,"created_at_ms":1718900000000}
]}

PUT /api/trust/peer
-> {"success":true,"data":{"id":"umni-b","permissions":["out","adc","ntc"]}}
```

## Порядок выдачи прав — tabela ответственности

| Этап                            | Кто действует   | На каком устройстве     | Какое действие                                                  | Права peer                                                    |
| ------------------------------- | --------------- | ----------------------- | --------------------------------------------------------------- | ------------------------------------------------------------- |
| 1. Генерация кода               | локальный админ | Устройство A            | `POST /api/trust/pair/generate`                                 | — (код только создаёт предварительный слот секрета)           |
| 2. Потребление кода + handshake | локальный админ | Устройство B            | `POST /api/trust/pair/consume` → `POST /api/trust/confirm` на A | Обе стороны добавляют друг друга с **пустыми** правами `[]`   |
| 3. Выдача прав B на ресурсы A   | локальный админ | Устройство A            | `PUT /api/trust/peer {"id":"B","permissions":[...]}`            | B получает указанные права на A                               |
| 4. Выдача прав A на ресурсы B   | локальный админ | Устройство B            | `PUT /api/trust/peer {"id":"A","permissions":[...]}`            | A получает указанные права на B                               |
| 5. Управление peer              | локальный админ | Инициатор               | `POST /api/trust/peer/state` или `peer/switch`                  | Используются права, выданные инициатору на целевом устройстве |
| 6. Удаление                     | локальный админ | Каждая сторона отдельно | `DELETE /api/trust/peer?id=…`                                   | Peer удаляется со стороны инициатора удаления                 |

## C API (публичные функции)

```c
#include "um_trust.h"

// Жизненный цикл
esp_err_t um_trust_init(void);                // после um_nvs_init/um_storage_init
esp_err_t um_trust_flush(void);               // форсированная запись trust.json на диск

// Пееры
const um_trust_peer_t *um_trust_get(const char *peer_id);
int um_trust_list(const um_trust_peer_t **out, int max);
esp_err_t um_trust_add_or_update(const um_trust_peer_t *peer);
esp_err_t um_trust_remove(const char *peer_id);
esp_err_t um_trust_set_perms(const char *peer_id, const char *const *perms, int n);
bool um_trust_peer_has(const char *peer_id, const char *cap);
esp_err_t um_trust_update_ip(const char *peer_id, const char *ip, uint16_t port);

// Сопряжение
esp_err_t um_trust_pair_generate(char *out_str, size_t out_len);   // "A:<base32>"
esp_err_t um_trust_pair_consume(const char *pair_str, char *out_peer_id, size_t out_peer_id_len);

// Подтверждение handshake (вызывается веб-обработчиком /api/trust/confirm)
esp_err_t um_trust_pair_confirm(httpd_req_t *req, const uint8_t *body, size_t body_len, um_auth_ctx_t *ctx);

// HMAC / подпись
esp_err_t um_trust_sign(const um_trust_peer_t *peer, const char *method, const char *path,
                        const uint8_t *body, size_t body_len,
                        char *out_sig, size_t sig_len, char *out_ts, size_t ts_len,
                        char *out_nonce, size_t nonce_len);
esp_err_t um_trust_verify_body(httpd_req_t *req, const uint8_t *body, size_t body_len, um_auth_ctx_t *ctx);
esp_err_t um_trust_verify_stream(httpd_req_t *req, um_auth_ctx_t *ctx, uint8_t out_expected_hash[32]);

// Проверка прав (вызывается из обработчиков веб-сервера)
bool um_trust_enforce(const um_auth_ctx_t *ctx, httpd_req_t *req, cJSON *input);

// Вызов peer-устройства (подписанный HTTP)
esp_err_t um_trust_call_peer(const char *peer_id, const char *method, const char *path,
                             const char *body, char **resp_out);

// Web-server handlers (регистрируются в um_webserver)
esp_err_t um_trust_ws_pairs_list, um_trust_ws_pair_generate, um_trust_ws_pair_consume,
          um_trust_ws_pair_confirm, um_trust_ws_peer_delete, um_trust_ws_peer_update,
          um_trust_ws_peer_state, um_trust_ws_peer_switch;
```

Лимиты (из `um_trust.h`):

- `UM_TRUST_MAX_PEERS = 8` — максимум доверенных устройств
- `UM_TRUST_PERMS_MAX = 16` — максимум прав на одного peer-а
- `UM_TRUST_SECRET_LEN = 32` байт (секрет пары)
- `UM_TRUST_ID_MAX = 32` — длина device_id
- Код сопряжения живёт 5 минут; до 4 кодов同時

## Хранение

Конфигурация хранится в `/spiffs/trust.json` (версия `UM_TRUST_CONFIG_VERSION = 1`). Пример:

```json
{
  "version": 1,
  "peers": [
    {
      "id": "umni-b",
      "name": "Котельная B",
      "secret": "<hex 64 символов>",
      "permissions": ["out", "adc"],
      "last_ip": "192.168.1.42",
      "last_port": 80,
      "last_seen_ms": 1719000000000,
      "created_at_ms": 1718900000000
    }
  ]
}
```

Запись на диск происходит синхронно при добавлении/обновлении/удалении peer-а и периодически фоновым таймером (раз в 30 с, если есть грязные изменения). mDNS-адреса обновляются раз в 5 минут.

## Замечания по ESP-IDF v6.0 / mbedTLS 4

В ESP-IDF v6.0 legacy API `mbedtls_md_hmac()` и `mbedtls_ct_memcmp()` скрыты за `MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS`. В `um_trust.c` этот макрос объявлен до подключения заголовков mbedTLS, а constant-time сравнение из `mbedtls/constant_time.h`.

## Связанные документы

- `components/um_trust/HMAC_ADMIN.md` — административный HMAC-доступ (вместо Bearer-токена), схема подписи, пример на Python.
- `components/um_trust/examples/rest_client.py` — готовый REST-клиент (admin HMAC / Bearer).
- `components/um_webserver/README.md` — базовый веб-сервер, регистрация хэндлеров.
- `components/um_capabilities/README.md` — перечень возможностей устройства (на quali можно выдавать права).
