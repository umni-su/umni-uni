# Admin HMAC

Административный доступ к API можно подтверждать HMAC-подписью вместо Bearer-токена.
Bearer (`Authorization: Bearer <token>` / `?token=<token>`) остаётся полностью
работоспособным — оба способа проверяются независимо.

## Ключ (PSK)

HMAC-ключ администратора — это **строка webserver-токена из NVS** (та же, что
задаётся через `set_token`, ключ `httptoken`). Ключом служат байты строки,
длина = `strlen(token)`. Если токен пуст — admin-HMAC выключен.

## Идентификатор

В заголовке `X-UMNI-Device` администратор указывает зарезервированное значение
`admin`. Другие устройства (`umni-xxxx`) идут по обычному peer-пути (capability).

## Каноническая строка

```
UMNI1\n{METHOD}\n{URI}\n{TS_MS}\n{NONCE_HEX}\n{SHA256_BODY_HEX}
```

* `METHOD` — верхний регистр (`GET`, `POST`, `PUT`, ...).
* `URI` — точный `req->uri`, **включая query-строку** (например `/api/state?x=1`).
* `TS_MS` — Unix-время в миллисекундах (десятичное). Требуется синхронизация
  часов по NTP: окно проверки `±60000` мс.
* `NONCE_HEX` — 16 случайных байт в hex (32 символа). Повтор запрещён (replay-ring).
* `SHA256_BODY_HEX` — SHA256 тела запроса в hex (64 символа). Для GET/пустого
  тела — SHA256 от пустой строки.

Подпись — `HMAC-SHA256(key, canonical)`, hex (64 символа).

## Заголовки запроса

| Заголовок            | Значение                        |
|----------------------|---------------------------------|
| `X-UMNI-Device`      | `admin`                         |
| `X-UMNI-Timestamp`   | `{TS_MS}`                       |
| `X-UMNI-Nonce`       | `{NONCE_HEX}`                   |
| `X-UMNI-Signature`   | `{HMAC_HEX}`                    |
| `X-UMNI-Body-SHA256` | только для OTA (см. ниже)        |

Успешная проверка даёт `UM_AUTH_LOCAL` — полный доступ ко всем эндпоинтам.

## OTA

Тело прошивки стримится и не может быть прочитано до проверки подписи. Поэтому
клиент заранее присылает SHA256 прошивки в заголовке `X-UMNI-Body-SHA256`, и это
значение подписывается в каноне (`SHA256_BODY_HEX`). Устройство проверяет
подпись по заголовку, затем считает SHA256 потока инкрементально и **до**
установки загрузочной партиции сравнивает результат; при несовпадении OTA
прерывается (`esp_ota_abort`).

## Пример (Python)

```python
import hashlib
import hmac
import os
import time

def sign(token: str, method: str, uri: str, body: bytes = b"",
         body_sha256_hex: str | None = None):
    ts = str(int(time.time() * 1000))
    nonce = os.urandom(16).hex()
    bh = body_sha256_hex or hashlib.sha256(body).hexdigest()
    canonical = f"UMNI1\n{method}\n{uri}\n{ts}\n{nonce}\n{bh}"
    sig = hmac.new(token.encode(), canonical.encode(), hashlib.sha256).hexdigest()
    return {
        "X-UMNI-Device": "admin",
        "X-UMNI-Timestamp": ts,
        "X-UMNI-Nonce": nonce,
        "X-UMNI-Signature": sig,
    }

def sign_ota(token: str, uri: str, firmware: bytes):
    bh = hashlib.sha256(firmware).hexdigest()
    headers = sign(token, "POST", uri, b"", body_sha256_hex=bh)
    headers["X-UMNI-Body-SHA256"] = bh
    return headers
```

Для GET тело пустое: `sign(token, "GET", "/api/configuration")`.
