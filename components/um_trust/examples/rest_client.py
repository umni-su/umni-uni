#!/usr/bin/env python3
"""Пример REST-клиента к устройству UMNI с админским HMAC (admin id + токен как PSK).

Примеры:
    set UMNI_TOKEN=Sa27za11NOV
    python rest_client.py GET  /api/systeminfo
    python rest_client.py GET  /api/conf --query section=adc
    python rest_client.py POST /api/switch --json "{\"mode\":\"outputs\",\"index\":0,\"level\":1}"
    python rest_client.py POST /api/ota/update --ota build/umni-uni.bin

Bearer-режим (если HMAC на устройстве недоступен): добавьте --bearer.
"""

import argparse
import hashlib
import hmac
import json
import os
import time
import urllib.parse
import urllib.request


ADMIN_ID = "admin"


def build_headers(psk: str, method: str, uri: str, body: bytes,
                  body_sha256_hex: str | None = None) -> dict:
    """Заголовки X-UMNI-* для admin-HMAC.

    uri — путь вместе с query (должен совпадать с req->uri на устройстве).
    """
    ts = str(int(time.time() * 1000))
    nonce = os.urandom(16).hex()
    bh = body_sha256_hex or hashlib.sha256(body).hexdigest()
    canonical = f"UMNI1\n{method}\n{uri}\n{ts}\n{nonce}\n{bh}"
    sig = hmac.new(psk.encode(), canonical.encode(), hashlib.sha256).hexdigest()
    return {
        "X-UMNI-Device": ADMIN_ID,
        "X-UMNI-Timestamp": ts,
        "X-UMNI-Nonce": nonce,
        "X-UMNI-Signature": sig,
    }


def request(host: str, psk: str, method: str, path: str,
            query: dict | None = None, json_body=None,
            ota: bytes | None = None, bearer: bool = False):
    uri = path
    if query:
        uri += "?" + urllib.parse.urlencode(query)

    headers = {}
    if bearer:
        # Резервный вариант авторизации
        headers["Authorization"] = f"Bearer {psk}"

    if ota is not None:
        data = ota
        headers["Content-Type"] = "application/octet-stream"
        if not bearer:
            bh = hashlib.sha256(ota).hexdigest()
            headers.update(build_headers(psk, method, uri, b"", body_sha256_hex=bh))
            headers["X-UMNI-Body-SHA256"] = bh
    elif json_body is not None:
        data = json_body if isinstance(json_body, bytes) else json.dumps(json_body).encode()
        headers["Content-Type"] = "application/json"
        if not bearer:
            headers.update(build_headers(psk, method, uri, data))
    else:
        data = None
        if not bearer:
            headers.update(build_headers(psk, method, uri, b""))

    req = urllib.request.Request(f"http://{host}{uri}", data=data,
                                 headers=headers, method=method)
    with urllib.request.urlopen(req, timeout=15) as resp:
        raw = resp.read().decode(errors="replace")
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        return raw


def main():
    ap = argparse.ArgumentParser(description="UMNI REST client (admin HMAC)")
    ap.add_argument("method", choices=["GET", "POST", "PUT", "DELETE"])
    ap.add_argument("path", help="например /api/systeminfo")
    ap.add_argument("--host", default=os.environ.get("UMNI_HOST", "192.168.4.1"))
    ap.add_argument("--token", default=os.environ.get("UMNI_TOKEN"),
                    help="webserver-токен (PSK); по умолчанию $UMNI_TOKEN")
    ap.add_argument("--query", action="append", default=[],
                    help="key=value (можно указывать несколько раз)")
    ap.add_argument("--json", dest="json_body",
                    help="тело запроса в виде JSON-строки")
    ap.add_argument("--json-file", dest="json_file",
                    help="файл с JSON-телом (удобно в PowerShell 5.1)")
    ap.add_argument("--ota", help="путь к .bin прошивки для /api/ota/update")
    ap.add_argument("--bearer", action="store_true",
                    help="использовать Bearer вместо HMAC")
    args = ap.parse_args()

    if not args.token:
        ap.error("нужен --token или переменная окружения UMNI_TOKEN")

    query = dict(kv.split("=", 1) for kv in args.query) if args.query else None
    json_body = None
    if args.json_file:
        with open(args.json_file, "rb") as f:
            json_body = f.read()
    elif args.json_body:
        json_body = json.loads(args.json_body)
    ota = None
    if args.ota:
        with open(args.ota, "rb") as f:
            ota = f.read()

    result = request(args.host, args.token, args.method.upper(), args.path,
                     query=query, json_body=json_body, ota=ota,
                     bearer=args.bearer)

    if isinstance(result, str):
        print(result)
    else:
        print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
