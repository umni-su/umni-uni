# UM Trust — детальное описание REST API

Все эндпоинты зарегистрированы в `components/um_webserver/um_webserver.c` через `um_webserver_register_*` и реализованы в `components/um_trust/um_trust.c` (функции `um_trust_ws_*`).

Общий формат ответов: `{"success": true|false, "data": {...}|null, "error": "..."}`.

## 1. GET /api/trust/pairs — список доверенных устройств

**Доступ:** только локальный админ (Bearer-токен или Admin-HMAC). Peer-устройству — `403 Forbidden`.

**Ответ:**
```http
GET /api/trust/pairs
Authorization: Bearer <токен>

-> {
  "success": true,
  "data": [
    {
      "id": "umni-b",
      "name": "Котельная B",
      "permissions": ["out", "adc"],
      "last_ip": "192.168.1.42",
      "last_port": 80,
      "last_seen_ms": 1719000000000,
      "created_at_ms": 1718900000000
    }
  ]
}
```

## 2. POST /api/trust/pair/generate — сгенерировать код сопряжения

**Доступ:** только локальный админ.

**Запрос:** без тела.

**Ответ:**
```http
POST /api/trust/pair/generate
Authorization: Bearer <токен>

-> {
  "success": true,
  "data": {
    "pair": "umni-a:JBSWY3DPEHPK3PXP...",
    "expires_in": 300
  }
}
```
`pair` имеет вид `<device_id_отправителя>:<base32(32 байт)>`. Срок жизни — 5 минут. Одновременно может быть не более 4 кодов.

## 3. POST /api/trust/pair/consume — принять код и сопрячься

**Доступ:** только локальный админ устройства, которое потребирует код.

**Запрос:**
```http
POST /api/trust/pair/consume
Authorization: Bearer <токен устройства-получателя>
Content-Type: application/json

{"pair":"umni-a:JBSWY3DPEHPK3PXP..."}
```

**Ответ:**
```http
-> {
  "success": true,
  "data": {
    "peer": "umni-a",
    "confirmed": true
  }
}
```
Что происходит внутри:
1. Получатель декодирует `device_id` и секрет из кода, сохраняет отправителя как peer (права = пусто).
2. Получатель сам вызывает `отправитель: POST /api/trust/confirm` с подписью этим секретом (взаимный handshake).
3. Если отправитель подтвердил — `confirmed: true`, и у обеих сторон появляется запись о другой.

## 4. POST /api/trust/confirm — подтверждение handshake (только peer)

**Доступ:** только в рамках пары сопряжения (вызывается устройством-получателем кода на устройство-отправитель). Обычный локальный админ или сторонний peer не должен вызывать этот эндпоинт напрямую.

**Заголовки:** `X-UMNI-Device`, `X-UMNI-Timestamp`, `X-UMNI-Nonce`, `X-UMNI-Signature` — подпись одним из неистёкших секретов пары.

**Ответ:**
```http
-> {"success": true, "data": {"peer": "<device_id_отправителя>", "paired": true}}
```

## 5. PUT /api/trust/peer — обновить имя и права trusted device

**Доступ:** только локальный админ устройства-владельца ресурса.

**Запрос:**
```http
PUT /api/trust/peer
Authorization: Bearer <токен устройства-владельца>
Content-Type: application/json

{
  "id": "umni-b",
  "name": "Котельная B",
  "permissions": ["out", "adc", "ntc"]
}
```

**Ответ:** обновлённая запись peer:
```http
-> {
  "success": true,
  "data": {
    "id": "umni-b",
    "name": "Котельная B",
    "permissions": ["out", "adc", "ntc"],
    ...
  }
}
```
Права заменяются полностью (не дозаписываются). Передайте пустой массив `[]`, чтобы отобрать все права.

## 6. DELETE /api/trust/peer?id=<id> — удаление trusted device

**Доступ:** только локальный админ устройства-владельца.

**Запрос:**
```http
DELETE /api/trust/peer?id=umni-b
Authorization: Bearer <токен устройства-владельца>
```

**Ответ:**
```http
-> {"success": true, "data": {"id": "umni-b"}}
```
Удаление одностороннее: удаляет peer только с этой стороны. Для полного разрыва доверия выполните `DELETE` на обоих устройствах (A удаляет B, B удаляет A).

## 7. POST /api/trust/peer/state — запросить состояние capability у peer

**Доступ:** только локальный админ устройства-инициатора. Инициатор должен иметь право `capability` на целевом устройстве (выдано целевым устройством через `PUT /api/trust/peer`).

**Запрос:**
```http
POST /api/trust/peer/state
Authorization: Bearer <токен инициатора>
Content-Type: application/json

{"id":"umni-b","capability":"ntc"}
```
Внутри инициатор вызывает `umni-b: POST /api/state {"capability":"ntc"}` с HMAC-подписью. Целевое устройство проверяет `um_trust_peer_has(инициатор, "ntc")`.

**Ответ:** пробрасывает ответ целевого устройства (`/api/state`), например:
```http
-> {"success": true, "data": {"value": 22.5, "unit": "celsius"}}
```

## 8. POST /api/trust/peer/switch — переключить выход у peer

**Доступ:** только локальный админ устройства-инициатора. Инициатор должен иметь право `out` (для `mode:outputs`) или `oc` (для `mode:opencollectors`) на целевом устройстве.

**Запрос:**
```http
POST /api/trust/peer/switch
Authorization: Bearer <токен инициатора>
Content-Type: application/json

{"id":"umni-b","mode":"outputs","index":1,"level":true}
```
`index` — 1-based. Внутри вызывается `umni-b: POST /api/switch {"mode":"outputs","index":1,"level":true}`.

**Ответ:** пробрасывает ответ целевого устройства (`/api/switch`).

---

## Таблица доступа (кратко)

| Эндпоинт | Локальный админ | Peer-устройство | Примечание |
|----------|-----------------|-----------------|------------|
| GET `/api/trust/pairs` | ✅ | ❌ 403 | — |
| POST `/api/trust/pair/generate` | ✅ | ❌ 403 | — |
| POST `/api/trust/pair/consume` | ✅ | ❌ 403 | инициирует mutual confirm |
| POST `/api/trust/confirm` | ⚠️ special | ✅ (handshake) | только внутри pairing |
| PUT `/api/trust/peer` | ✅ | ❌ 403 | выдача прав |
| DELETE `/api/trust/peer?id=` | ✅ | ❌ 403 | одностороннее удаление |
| POST `/api/trust/peer/state` | ✅ | ❌ 403 | вызов peer: `/api/state` |
| POST `/api/trust/peer/switch` | ✅ | ❌ 403 | вызов peer: `/api/switch` |

## Связь с другими компонентами

- `um_webserver` — регистрирует хэндлеры `/api/trust/*` и вызывает `um_trust_enforce` для проверки прав peer-а на wszystkich не-trust эндпоинтах (`/api/state`, `/api/switch`, `/api/beep`, `/api/rf/scan`, `/api/conf`, `/api/systeminfo`).
- `um_capabilities` — перечисляет возможности устройства;権限 строка в trust соответствует capability, которой может владеть устройство.
- `HMAC_ADMIN.md` — альтернатива Bearer-токену для админ-доступа.
