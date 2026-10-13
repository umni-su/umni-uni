// mbedTLS 4 (IDF v6.0) hides the legacy MD/HMAC and constant-time helpers
// behind MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS. IDF itself uses this trick in
// its port wrappers (port/include/mbedtls/bignum.h, ecp.h), so we mirror it.
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS

#include "um_trust.h"
#include "um_helpers.h"
#include "um_storage.h"
#include "um_capabilities.h"
#include "um_sockets.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_http_client.h"
#include "mbedtls/md.h"
#include "mbedtls/constant_time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

static const char *TAG = "um_trust";

/* ---------- state ---------- */
static um_trust_peer_t s_peers[UM_TRUST_MAX_PEERS];
static int s_peer_count = 0;
static SemaphoreHandle_t s_lock = NULL;
static char s_self_id[UM_TRUST_ID_MAX];
static TimerHandle_t s_refresh_timer = NULL;
static TimerHandle_t s_save_timer = NULL;
static volatile bool s_dirty = false;

/* Nonce cache */
#define NONCE_RING 64
typedef struct
{
    char peer[UM_TRUST_ID_MAX];
    uint8_t n[UM_TRUST_NONCE_LEN];
    int64_t ts;
} nonce_slot_t;
static nonce_slot_t s_nonces[NONCE_RING];
static int s_nonce_head = 0;

/* Одноразовые секреты сопряжения (живут 5 минут) */
#define PAIR_SLOTS 4
typedef struct
{
    uint8_t secret[UM_TRUST_SECRET_LEN];
    int64_t expires_ms;
    bool used;
} pair_slot_t;
static pair_slot_t s_pairs[PAIR_SLOTS];

/* ---------- utils ---------- */
static int64_t now_ms(void) { return (int64_t)um_helpers_get_real_timestamp_ms(); }

static void bin_to_hex(const uint8_t *in, size_t n, char *out)
{
    static const char *h = "0123456789abcdef";
    for (size_t i = 0; i < n; i++)
    {
        out[i * 2] = h[in[i] >> 4];
        out[i * 2 + 1] = h[in[i] & 0xF];
    }
    out[n * 2] = 0;
}

static int hex_to_bin(const char *hex, uint8_t *out, size_t out_max)
{
    size_t n = strlen(hex);
    if (n % 2 || n / 2 > out_max)
        return -1;
    for (size_t i = 0; i < n / 2; i++)
    {
        unsigned v;
        if (sscanf(hex + i * 2, "%2x", &v) != 1)
            return -1;
        out[i] = (uint8_t)v;
    }
    return (int)(n / 2);
}

/* =====================================================================
   SPIFFS persistence  (/spiffs/trust.json)
   ===================================================================== */

/* Загрузка файла → s_peers[]. Вызывается один раз при init. */
static esp_err_t spiffs_load(void)
{
    s_peer_count = 0;

    if (!um_storage_file_exists(UM_TRUST_CONFIG_PATH))
    {
        ESP_LOGI(TAG, "No trust.json yet, starting empty");
        return ESP_OK;
    }

    char *json = um_storage_read_json_string(UM_TRUST_CONFIG_PATH);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to read %s", UM_TRUST_CONFIG_PATH);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root)
    {
        ESP_LOGE(TAG, "Failed to parse trust.json");
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *peers = cJSON_GetObjectItem(root, "peers");
    if (cJSON_IsArray(peers))
    {
        cJSON *it = NULL;
        cJSON_ArrayForEach(it, peers)
        {
            if (s_peer_count >= UM_TRUST_MAX_PEERS)
            {
                ESP_LOGW(TAG, "Too many peers, truncated at %d", UM_TRUST_MAX_PEERS);
                break;
            }
            um_trust_peer_t *p = &s_peers[s_peer_count];

            cJSON *id = cJSON_GetObjectItem(it, "id");
            cJSON *name = cJSON_GetObjectItem(it, "name");
            cJSON *sec = cJSON_GetObjectItem(it, "secret");
            cJSON *perm = cJSON_GetObjectItem(it, "permissions");
            cJSON *ip = cJSON_GetObjectItem(it, "last_ip");
            cJSON *port = cJSON_GetObjectItem(it, "last_port");
            cJSON *seen = cJSON_GetObjectItem(it, "last_seen_ms");
            cJSON *crea = cJSON_GetObjectItem(it, "created_at_ms");

            if (!cJSON_IsString(id))
                continue;

            memset(p, 0, sizeof(*p));
            strlcpy(p->id, id->valuestring, sizeof(p->id));
            if (cJSON_IsString(name))
                strlcpy(p->name, name->valuestring, sizeof(p->name));

            if (cJSON_IsString(sec))
            {
                if (hex_to_bin(sec->valuestring, p->secret, UM_TRUST_SECRET_LEN) != (int)UM_TRUST_SECRET_LEN)
                {
                    ESP_LOGW(TAG, "Peer %s: bad secret length, skipping", p->id);
                    continue;
                }
            }
            else
            {
                ESP_LOGW(TAG, "Peer %s: no secret, skipping", p->id);
                continue;
            }

            if (cJSON_IsArray(perm))
            {
                int n = cJSON_GetArraySize(perm);
                if (n > UM_TRUST_PERMS_MAX)
                    n = UM_TRUST_PERMS_MAX;
                for (int i = 0; i < n; i++)
                {
                    cJSON *s = cJSON_GetArrayItem(perm, i);
                    if (cJSON_IsString(s))
                        strlcpy(p->perms[p->perms_count++],
                                s->valuestring, UM_TRUST_PERM_LEN_MAX);
                }
            }

            if (cJSON_IsString(ip))
                strlcpy(p->last_ip, ip->valuestring, sizeof(p->last_ip));
            if (cJSON_IsNumber(port))
                p->last_port = (uint16_t)port->valueint;
            if (cJSON_IsNumber(seen))
                p->last_seen_ms = (int64_t)seen->valuedouble;
            if (cJSON_IsNumber(crea))
                p->created_at_ms = (int64_t)crea->valuedouble;

            ESP_LOGI(TAG, "Loaded peer %s (%d perms)", p->id, p->perms_count);
            s_peer_count++;
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d peers from SPIFFS", s_peer_count);
    return ESP_OK;
}

/* Сохранение s_peers[] → файл. Вызывать под s_lock или из одного контекста. */
static esp_err_t spiffs_save_locked(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return ESP_ERR_NO_MEM;

    cJSON_AddNumberToObject(root, "version", UM_TRUST_CONFIG_VERSION);
    cJSON *peers = cJSON_CreateArray();

    for (int i = 0; i < s_peer_count; i++)
    {
        const um_trust_peer_t *p = &s_peers[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", p->id);
        cJSON_AddStringToObject(o, "name", p->name);

        char hex[UM_TRUST_SECRET_LEN * 2 + 1];
        bin_to_hex(p->secret, UM_TRUST_SECRET_LEN, hex);
        cJSON_AddStringToObject(o, "secret", hex);

        cJSON *perms = cJSON_CreateArray();
        for (int j = 0; j < p->perms_count; j++)
            cJSON_AddItemToArray(perms, cJSON_CreateString(p->perms[j]));
        cJSON_AddItemToObject(o, "permissions", perms);

        cJSON_AddStringToObject(o, "last_ip", p->last_ip);
        cJSON_AddNumberToObject(o, "last_port", p->last_port);
        cJSON_AddNumberToObject(o, "last_seen_ms", (double)p->last_seen_ms);
        cJSON_AddNumberToObject(o, "created_at_ms", (double)p->created_at_ms);

        cJSON_AddItemToArray(peers, o);
    }

    cJSON_AddItemToObject(root, "peers", peers);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_str)
        return ESP_ERR_NO_MEM;

    esp_err_t e = um_storage_write_json(UM_TRUST_CONFIG_PATH, json_str);
    free(json_str);
    if (e == ESP_OK)
        s_dirty = false;
    return e;
}

/* Публичный flush (например, из кнопки или при завершении работы) */
esp_err_t um_trust_flush(void)
{
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    esp_err_t e = spiffs_save_locked();
    xSemaphoreGive(s_lock);
    return e;
}

/* =====================================================================
   nonce replay
   ===================================================================== */
static bool nonce_seen_and_add(const char *peer, const uint8_t *n, int64_t ts)
{
    int64_t cutoff = ts - UM_TRUST_TS_WINDOW_MS;
    for (int i = 0; i < NONCE_RING; i++)
        if (s_nonces[i].ts > 0 && s_nonces[i].ts < cutoff)
            s_nonces[i].ts = 0;

    for (int i = 0; i < NONCE_RING; i++)
    {
        if (s_nonces[i].ts > 0 &&
            strcmp(s_nonces[i].peer, peer) == 0 &&
            memcmp(s_nonces[i].n, n, UM_TRUST_NONCE_LEN) == 0)
            return true;
    }
    nonce_slot_t *slot = &s_nonces[s_nonce_head];
    strlcpy(slot->peer, peer, sizeof(slot->peer));
    memcpy(slot->n, n, UM_TRUST_NONCE_LEN);
    slot->ts = ts;
    s_nonce_head = (s_nonce_head + 1) % NONCE_RING;
    return false;
}

/* =====================================================================
   HMAC
   ===================================================================== */
static esp_err_t hmac_sha256(const uint8_t *key, size_t key_len,
                             const uint8_t *data, size_t data_len,
                             uint8_t out[32])
{
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return mbedtls_md_hmac(md, key, key_len, data, data_len, out) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t um_trust_sign(const um_trust_peer_t *peer,
                        const char *method, const char *path,
                        const uint8_t *body, size_t body_len,
                        char *out_sig, size_t sig_len,
                        char *out_ts, size_t ts_len,
                        char *out_nonce, size_t nonce_len)
{
    if (!peer || !method || !path)
        return ESP_ERR_INVALID_ARG;
    if (sig_len < UM_TRUST_SIG_HEX_LEN ||
        ts_len < 24 ||
        nonce_len < UM_TRUST_NONCE_LEN * 2 + 1)
        return ESP_ERR_INVALID_ARG;

    uint8_t nonce[UM_TRUST_NONCE_LEN];
    esp_fill_random(nonce, sizeof(nonce));
    bin_to_hex(nonce, sizeof(nonce), out_nonce);

    int64_t ts = now_ms();
    snprintf(out_ts, ts_len, "%lld", (long long)ts);

    uint8_t body_hash[32];
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md(md, body, body_len, body_hash);
    char body_hash_hex[65];
    bin_to_hex(body_hash, 32, body_hash_hex);

    char canonical[768];
    snprintf(canonical, sizeof(canonical),
             "UMNI1\n%s\n%s\n%s\n%s\n%s",
             method, path, out_ts, out_nonce, body_hash_hex);

    uint8_t mac[32];
    if (hmac_sha256(peer->secret, UM_TRUST_SECRET_LEN,
                    (const uint8_t *)canonical, strlen(canonical), mac) != ESP_OK)
        return ESP_FAIL;

    bin_to_hex(mac, 32, out_sig);
    return ESP_OK;
}

esp_err_t um_trust_verify_body(httpd_req_t *req,
                               const uint8_t *body, size_t body_len,
                               um_auth_ctx_t *ctx)
{
    ctx->kind = UM_AUTH_NONE;

    char dev[UM_TRUST_ID_MAX] = {0};
    char ts_s[24] = {0};
    char nonce_s[UM_TRUST_NONCE_LEN * 2 + 1] = {0};
    char sig_s[UM_TRUST_SIG_HEX_LEN] = {0};

    if (httpd_req_get_hdr_value_str(req, "X-UMNI-Device", dev, sizeof(dev)) != ESP_OK)
        return ESP_ERR_NOT_FOUND;
    if (httpd_req_get_hdr_value_str(req, "X-UMNI-Timestamp", ts_s, sizeof(ts_s)) != ESP_OK)
        return ESP_ERR_NOT_FOUND;
    if (httpd_req_get_hdr_value_str(req, "X-UMNI-Nonce", nonce_s, sizeof(nonce_s)) != ESP_OK)
        return ESP_ERR_NOT_FOUND;
    if (httpd_req_get_hdr_value_str(req, "X-UMNI-Signature", sig_s, sizeof(sig_s)) != ESP_OK)
        return ESP_ERR_NOT_FOUND;

    const um_trust_peer_t *peer = um_trust_get(dev);
    if (!peer)
        return ESP_ERR_NOT_FOUND;

    int64_t ts = atoll(ts_s);
    int64_t now = now_ms();
    if (llabs(now - ts) > UM_TRUST_TS_WINDOW_MS)
        return ESP_ERR_INVALID_STATE;

    uint8_t nonce[UM_TRUST_NONCE_LEN];
    if (hex_to_bin(nonce_s, nonce, sizeof(nonce)) != (int)sizeof(nonce))
        return ESP_ERR_INVALID_ARG;
    if (nonce_seen_and_add(dev, nonce, now))
        return ESP_ERR_INVALID_STATE; // replay

    uint8_t body_hash[32];
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md(md, body, body_len, body_hash);
    char body_hash_hex[65];
    bin_to_hex(body_hash, 32, body_hash_hex);

    char canonical[768];
    snprintf(canonical, sizeof(canonical),
             "UMNI1\n%s\n%s\n%s\n%s\n%s",
             http_method_str(req->method), req->uri,
             ts_s, nonce_s, body_hash_hex);

    uint8_t mac[32];
    if (hmac_sha256(peer->secret, UM_TRUST_SECRET_LEN,
                    (const uint8_t *)canonical, strlen(canonical), mac) != ESP_OK)
        return ESP_FAIL;

    uint8_t given[32];
    if (hex_to_bin(sig_s, given, sizeof(given)) != (int)sizeof(given))
        return ESP_ERR_INVALID_ARG;
    if (mbedtls_ct_memcmp(mac, given, 32) != 0)
        return ESP_ERR_INVALID_STATE;

    ctx->kind = UM_AUTH_PEER;
    strlcpy(ctx->peer_id, dev, sizeof(ctx->peer_id));
    return ESP_OK;
}

/* =====================================================================
   Enforcement
   ===================================================================== */
static const char *section_to_perm(const char *section)
{
    if (!section)
        return NULL;
    if (!strcmp(section, "onewire"))
        return "onewire";
    if (!strcmp(section, "inputs"))
        return "in";
    if (!strcmp(section, "outputs"))
        return "out";
    if (!strcmp(section, "opencollectors"))
        return "oc";
    if (!strcmp(section, "rf433"))
        return "rf433";
    if (!strcmp(section, "adc") ||
        !strcmp(section, "ai"))
        return "adc";
    if (!strcmp(section, "ntc"))
        return "ntc";
    return NULL;
}

bool um_trust_enforce(const um_auth_ctx_t *ctx, httpd_req_t *req, cJSON *input)
{
    if (ctx->kind == UM_AUTH_LOCAL)
        return true;
    if (ctx->kind != UM_AUTH_PEER)
        return false;

    const char *uri = req->uri;

    /* Peer'ам запрещено всё «административное» */
    if (!strncmp(uri, "/api/configuration", 18))
        return false;
    if (!strncmp(uri, "/api/reboot", 11))
        return false;
    if (!strncmp(uri, "/api/ota", 8))
        return false;
    if (!strncmp(uri, "/api/trust", 9))
        return false;
    if (!strncmp(uri, "/api/automations", 16))
        return false;
    if (!strncmp(uri, "/api/settings", 13))
        return false;
    if (!strncmp(uri, "/api/login", 10))
        return false;

    /* POST /api/state */
    if (!strncmp(uri, "/api/state", 10))
    {
        cJSON *cap = input ? cJSON_GetObjectItem(input, "capability") : NULL;
        if (!cJSON_IsString(cap))
            return false;
        char perm[UM_TRUST_PERM_LEN_MAX];
        strlcpy(perm, cap->valuestring, sizeof(perm));
        size_t l = strlen(perm);
        if (l && perm[l - 1] >= '0' && perm[l - 1] <= '9')
            perm[l - 1] = 0;
        return um_trust_peer_has(ctx->peer_id, perm);
    }

    /* POST /api/switch */
    if (!strncmp(uri, "/api/switch", 11))
    {
        cJSON *mode = input ? cJSON_GetObjectItem(input, "mode") : NULL;
        if (!cJSON_IsString(mode))
            return false;
        if (!strcmp(mode->valuestring, "outputs"))
            return um_trust_peer_has(ctx->peer_id, "out");
        if (!strcmp(mode->valuestring, "opencollectors"))
            return um_trust_peer_has(ctx->peer_id, "oc");
        return false;
    }

    if (!strncmp(uri, "/api/beep", 9))
        return um_trust_peer_has(ctx->peer_id, "buzzer");
    if (!strncmp(uri, "/api/rf/scan", 12))
        return um_trust_peer_has(ctx->peer_id, "rf433");

    /* GET /api/conf?section=… */
    if (!strncmp(uri, "/api/conf", 9))
    {
        char q[128] = {0}, section[32] = {0};
        if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK)
            httpd_query_key_value(q, "section", section, sizeof(section));
        const char *perm = section_to_perm(section);
        return perm && um_trust_peer_has(ctx->peer_id, perm);
    }

    if (!strncmp(uri, "/api/systeminfo", 15))
        return true;
    if (!strncmp(uri, "/api/wifi/scan", 14))
        return false;

    return false;
}

/* =====================================================================
   Pairing (PSK, base32)
   ===================================================================== */
static const char B32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static void b32_encode(const uint8_t *in, size_t n, char *out)
{
    size_t o = 0;
    uint32_t buf = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++)
    {
        buf = (buf << 8) | in[i];
        bits += 8;
        while (bits >= 5)
        {
            out[o++] = B32[(buf >> (bits - 5)) & 0x1F];
            bits -= 5;
        }
    }
    if (bits > 0)
        out[o++] = B32[(buf << (5 - bits)) & 0x1F];
    out[o] = 0;
}

static int b32_decode(const char *in, uint8_t *out, size_t out_max)
{
    uint32_t buf = 0;
    int bits = 0;
    size_t o = 0;
    for (const char *p = in; *p; p++)
    {
        const char *q = strchr(B32, *p);
        if (!q)
            continue;
        buf = (buf << 5) | (uint32_t)(q - B32);
        bits += 5;
        if (bits >= 8)
        {
            if (o >= out_max)
                return -1;
            out[o++] = (buf >> (bits - 8)) & 0xFF;
            bits -= 8;
        }
    }
    return (int)o;
}

esp_err_t um_trust_pair_generate(char *out_str, size_t out_len)
{
    if (!out_str || out_len < 80)
        return ESP_ERR_INVALID_ARG;
    int slot = -1;
    for (int i = 0; i < PAIR_SLOTS; i++)
        if (!s_pairs[i].used)
        {
            slot = i;
            break;
        }
    if (slot < 0)
        return ESP_ERR_NO_MEM;

    pair_slot_t *ps = &s_pairs[slot];
    esp_fill_random(ps->secret, sizeof(ps->secret));
    ps->expires_ms = now_ms() + 5LL * 60 * 1000;
    ps->used = false;

    char b32[64] = {0};
    b32_encode(ps->secret, sizeof(ps->secret), b32);

    snprintf(out_str, out_len, "%s:%s", s_self_id, b32);
    ESP_LOGI(TAG, "Pair string generated (valid 5 min)");
    return ESP_OK;
}

esp_err_t um_trust_pair_consume(const char *pair_str,
                                char *out_peer_id, size_t out_peer_id_len)
{
    if (!pair_str)
        return ESP_ERR_INVALID_ARG;
    const char *colon = strchr(pair_str, ':');
    if (!colon)
        return ESP_ERR_INVALID_ARG;

    size_t id_len = colon - pair_str;
    if (id_len == 0 || id_len >= UM_TRUST_ID_MAX)
        return ESP_ERR_INVALID_ARG;

    uint8_t secret[UM_TRUST_SECRET_LEN];
    if (b32_decode(colon + 1, secret, sizeof(secret)) != (int)sizeof(secret))
        return ESP_ERR_INVALID_ARG;

    um_trust_peer_t p = {0};
    strlcpy(p.id, pair_str, id_len + 1);
    strlcpy(p.name, p.id, sizeof(p.name));
    memcpy(p.secret, secret, sizeof(secret));
    p.perms_count = 0;
    p.created_at_ms = now_ms();

    if (out_peer_id && out_peer_id_len)
        strlcpy(out_peer_id, p.id, out_peer_id_len);
    return um_trust_add_or_update(&p);
}

/* =====================================================================
   Peers CRUD
   ===================================================================== */
const um_trust_peer_t *um_trust_get(const char *peer_id)
{
    if (!peer_id)
        return NULL;
    for (int i = 0; i < s_peer_count; i++)
        if (!strcmp(s_peers[i].id, peer_id))
            return &s_peers[i];
    return NULL;
}

int um_trust_list(const um_trust_peer_t **out, int max)
{
    int n = s_peer_count < max ? s_peer_count : max;
    for (int i = 0; i < n; i++)
        out[i] = &s_peers[i];
    return n;
}

esp_err_t um_trust_add_or_update(const um_trust_peer_t *peer)
{
    if (!peer || !peer->id[0])
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    esp_err_t ret = ESP_OK;
    int idx = -1;
    for (int i = 0; i < s_peer_count; i++)
        if (!strcmp(s_peers[i].id, peer->id))
        {
            idx = i;
            break;
        }

    if (idx < 0)
    {
        if (s_peer_count >= UM_TRUST_MAX_PEERS)
        {
            ret = ESP_ERR_NO_MEM;
            goto out;
        }
        idx = s_peer_count++;
    }
    s_peers[idx] = *peer;
    s_dirty = true;
    ret = spiffs_save_locked(); /* синхронно сохраняем — операция редкая */

out:
    xSemaphoreGive(s_lock);
    return ret;
}

esp_err_t um_trust_remove(const char *peer_id)
{
    if (!peer_id)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    int idx = -1;
    for (int i = 0; i < s_peer_count; i++)
        if (!strcmp(s_peers[i].id, peer_id))
        {
            idx = i;
            break;
        }

    if (idx < 0)
    {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }

    for (int i = idx; i < s_peer_count - 1; i++)
        s_peers[i] = s_peers[i + 1];
    s_peer_count--;
    s_dirty = true;
    esp_err_t e = spiffs_save_locked();

    xSemaphoreGive(s_lock);
    return e;
}

esp_err_t um_trust_set_perms(const char *peer_id, const char *const *perms, int n)
{
    if (!peer_id)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    int idx = -1;
    for (int i = 0; i < s_peer_count; i++)
        if (!strcmp(s_peers[i].id, peer_id))
        {
            idx = i;
            break;
        }

    if (idx < 0)
    {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }

    if (n > UM_TRUST_PERMS_MAX)
        n = UM_TRUST_PERMS_MAX;
    s_peers[idx].perms_count = 0;
    for (int i = 0; i < n; i++)
    {
        if (!perms[i])
            continue;
        strlcpy(s_peers[idx].perms[s_peers[idx].perms_count],
                perms[i], UM_TRUST_PERM_LEN_MAX);
        s_peers[idx].perms_count++;
    }
    s_dirty = true;
    esp_err_t e = spiffs_save_locked();

    xSemaphoreGive(s_lock);
    return e;
}

bool um_trust_peer_has(const char *peer_id, const char *cap)
{
    if (!peer_id || !cap)
        return false;
    const um_trust_peer_t *p = um_trust_get(peer_id);
    if (!p)
        return false;
    for (int i = 0; i < p->perms_count; i++)
        if (!strcmp(p->perms[i], cap))
            return true;
    return false;
}

/* IP обновляем часто — пишем только в RAM, dirty ставим,
   на диск сбрасываем периодически таймером */
esp_err_t um_trust_update_ip(const char *peer_id,
                             const char *ip, uint16_t port)
{
    if (!peer_id || !ip)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    int idx = -1;
    for (int i = 0; i < s_peer_count; i++)
        if (!strcmp(s_peers[i].id, peer_id))
        {
            idx = i;
            break;
        }

    if (idx < 0)
    {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }

    if (strcmp(s_peers[idx].last_ip, ip) != 0 || s_peers[idx].last_port != port)
    {
        strlcpy(s_peers[idx].last_ip, ip, sizeof(s_peers[idx].last_ip));
        s_peers[idx].last_port = port ? port : 80;
        s_dirty = true;
    }
    s_peers[idx].last_seen_ms = now_ms();

    xSemaphoreGive(s_lock);
    return ESP_OK;
}

/* =====================================================================
   Background refresh + dirty-save timer
   ===================================================================== */
static void refresh_timer_cb(TimerHandle_t t)
{
    (void)t;
    for (int i = 0; i < s_peer_count; i++)
    {
        char ip[UM_TRUST_IP_LEN] = {0};
        uint16_t port = 80;
        if (um_sockets_resolve(s_peers[i].id, ip, sizeof(ip), &port) == ESP_OK)
        {
            if (strcmp(ip, s_peers[i].last_ip) != 0)
            {
                ESP_LOGI(TAG, "Peer %s: %s -> %s", s_peers[i].id, s_peers[i].last_ip, ip);
                um_trust_update_ip(s_peers[i].id, ip, port);
            }
            else
            {
                s_peers[i].last_seen_ms = now_ms();
            }
        }
    }
}

static void save_timer_cb(TimerHandle_t t)
{
    (void)t;
    if (!s_dirty)
        return;
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE)
        return;
    if (s_dirty)
    {
        esp_err_t e = spiffs_save_locked();
        if (e == ESP_OK)
            ESP_LOGD(TAG, "trust.json flushed (dirty)");
    }
    xSemaphoreGive(s_lock);
}

/* =====================================================================
   Init
   ===================================================================== */
esp_err_t um_trust_init(void)
{
    if (s_lock)
        return ESP_OK;

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock)
        return ESP_ERR_NO_MEM;

    char *hn = NULL;
    if (um_helpers_get_hostname(&hn) == ESP_OK && hn)
    {
        strlcpy(s_self_id, hn, sizeof(s_self_id));
        free(hn);
    }
    else
    {
        strlcpy(s_self_id, "umni-unknown", sizeof(s_self_id));
    }

    /* SPIFFS должен быть уже смонтирован выше (um_storage_init) */
    spiffs_load();

    /* mDNS-refresh раз в 5 минут */
    s_refresh_timer = xTimerCreate("um_trust_ref",
                                   pdMS_TO_TICKS(5 * 60 * 1000),
                                   pdTRUE, NULL, refresh_timer_cb);
    if (s_refresh_timer)
        xTimerStart(s_refresh_timer, 0);

    /* dirty-save раз в 30 секунд — на случай обновления last_ip */
    s_save_timer = xTimerCreate("um_trust_save",
                                pdMS_TO_TICKS(30 * 1000),
                                pdTRUE, NULL, save_timer_cb);
    if (s_save_timer)
        xTimerStart(s_save_timer, 0);

    ESP_LOGI(TAG, "Trust init: self=%s, peers=%d", s_self_id, s_peer_count);
    return ESP_OK;
}

/* =====================================================================
   Peer-confirmed pairing (приём «висящего» секрета)
   ===================================================================== */
esp_err_t um_trust_pair_confirm(httpd_req_t *req,
                                const uint8_t *body, size_t body_len,
                                um_auth_ctx_t *ctx)
{
    if (!req)
        return ESP_ERR_INVALID_ARG;

    char dev[UM_TRUST_ID_MAX] = {0};
    char ts_s[24] = {0};
    char nonce_s[UM_TRUST_NONCE_LEN * 2 + 1] = {0};
    char sig_s[UM_TRUST_SIG_HEX_LEN] = {0};

    if (httpd_req_get_hdr_value_str(req, "X-UMNI-Device", dev, sizeof(dev)) != ESP_OK ||
        httpd_req_get_hdr_value_str(req, "X-UMNI-Timestamp", ts_s, sizeof(ts_s)) != ESP_OK ||
        httpd_req_get_hdr_value_str(req, "X-UMNI-Nonce", nonce_s, sizeof(nonce_s)) != ESP_OK ||
        httpd_req_get_hdr_value_str(req, "X-UMNI-Signature", sig_s, sizeof(sig_s)) != ESP_OK)
        return ESP_ERR_NOT_FOUND;

    int64_t ts = atoll(ts_s);
    int64_t now = now_ms();
    if (llabs(now - ts) > UM_TRUST_TS_WINDOW_MS)
        return ESP_ERR_INVALID_STATE;

    uint8_t nonce[UM_TRUST_NONCE_LEN];
    if (hex_to_bin(nonce_s, nonce, sizeof(nonce)) != (int)sizeof(nonce))
        return ESP_ERR_INVALID_ARG;

    uint8_t body_hash[32];
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md(md, body, body_len, body_hash);
    char body_hash_hex[65];
    bin_to_hex(body_hash, 32, body_hash_hex);

    char canonical[768];
    snprintf(canonical, sizeof(canonical),
             "UMNI1\n%s\n%s\n%s\n%s\n%s",
             http_method_str(req->method), req->uri,
             ts_s, nonce_s, body_hash_hex);

    uint8_t given[32];
    if (hex_to_bin(sig_s, given, sizeof(given)) != (int)sizeof(given))
        return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < PAIR_SLOTS; i++)
    {
        if (s_pairs[i].used || s_pairs[i].expires_ms <= now)
            continue;

        uint8_t mac[32];
        if (hmac_sha256(s_pairs[i].secret, UM_TRUST_SECRET_LEN,
                        (const uint8_t *)canonical, strlen(canonical), mac) != ESP_OK)
            continue;
        if (mbedtls_ct_memcmp(mac, given, 32) != 0)
            continue;

        /* Секрет совпал — одноразовый, помечаем использованным и регистрируем peer */
        s_pairs[i].used = true;

        um_trust_peer_t p = {0};
        strlcpy(p.id, dev, sizeof(p.id));
        strlcpy(p.name, dev, sizeof(p.name));
        memcpy(p.secret, s_pairs[i].secret, UM_TRUST_SECRET_LEN);
        p.perms_count = 0;
        p.last_seen_ms = now;
        p.created_at_ms = now;

        if (um_trust_add_or_update(&p) != ESP_OK)
            return ESP_FAIL;

        if (ctx)
        {
            ctx->kind = UM_AUTH_PEER;
            strlcpy(ctx->peer_id, dev, sizeof(ctx->peer_id));
        }
        ESP_LOGI(TAG, "Paired: registered peer %s", dev);
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

/* =====================================================================
   HTTP-клиент к peer'у (резолв id->IP, подпись, запрос)
   ===================================================================== */
static esp_err_t peer_resolve_addr(const um_trust_peer_t *peer,
                                   char *ip, size_t ip_len, uint16_t *port)
{
    *port = peer->last_port ? peer->last_port : 80;

    if (um_sockets_resolve(peer->id, ip, ip_len, port) == ESP_OK)
    {
        um_trust_update_ip(peer->id, ip, *port);
        return ESP_OK;
    }

    /* mDNS не ответил — используем кэш предыдущего адреса */
    if (peer->last_ip[0])
    {
        strlcpy(ip, peer->last_ip, ip_len);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t um_trust_call_peer(const char *peer_id,
                             const char *method,
                             const char *path,
                             const char *body,
                             char **resp_out)
{
    if (!peer_id || !method || !path)
        return ESP_ERR_INVALID_ARG;

    const um_trust_peer_t *peer = um_trust_get(peer_id);
    if (!peer)
        return ESP_ERR_NOT_FOUND;

    char ip[UM_TRUST_IP_LEN] = {0};
    uint16_t port = 80;
    if (peer_resolve_addr(peer, ip, sizeof(ip), &port) != ESP_OK)
    {
        ESP_LOGW(TAG, "Cannot resolve peer %s", peer_id);
        return ESP_ERR_NOT_FOUND;
    }

    const uint8_t *b = (const uint8_t *)(body ? body : "");
    size_t blen = body ? strlen(body) : 0;

    char sig[UM_TRUST_SIG_HEX_LEN];
    char ts[24];
    char nonce[UM_TRUST_NONCE_LEN * 2 + 1];
    if (um_trust_sign(peer, method, path, b, blen,
                      sig, sizeof(sig), ts, sizeof(ts), nonce, sizeof(nonce)) != ESP_OK)
        return ESP_FAIL;

    char url[192];
    snprintf(url, sizeof(url), "http://%s:%u%s", ip, (unsigned)port, path);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
        return ESP_ERR_NO_MEM;

    esp_http_client_set_header(client, "X-UMNI-Device", s_self_id);
    esp_http_client_set_header(client, "X-UMNI-Timestamp", ts);
    esp_http_client_set_header(client, "X-UMNI-Nonce", nonce);
    esp_http_client_set_header(client, "X-UMNI-Signature", sig);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (blen > 0)
        esp_http_client_set_post_field(client, body, blen);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Call %s%s failed: %s", ip, path, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    int total = esp_http_client_get_content_length(client);
    if (total < 0)
        total = 0;
    if (total > 8192)
        total = 8192;

    if (resp_out)
    {
        char *buf = calloc(1, total + 1);
        if (!buf)
        {
            esp_http_client_cleanup(client);
            return ESP_ERR_NO_MEM;
        }
        int rd = esp_http_client_read(client, buf, total);
        if (rd < 0)
            rd = 0;
        buf[rd] = 0;
        *resp_out = buf;
    }

    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300)
    {
        ESP_LOGW(TAG, "Peer %s %s -> HTTP %d", peer_id, path, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* =====================================================================
   Web-server handlers for /api/trust endpoints
   ===================================================================== */
static cJSON *peer_to_json(const um_trust_peer_t *p)
{
    cJSON *o = cJSON_CreateObject();
    if (!o)
        return NULL;
    cJSON_AddStringToObject(o, "id", p->id);
    cJSON_AddStringToObject(o, "name", p->name);

    cJSON *perms = cJSON_CreateArray();
    for (int i = 0; i < p->perms_count; i++)
        cJSON_AddItemToArray(perms, cJSON_CreateString(p->perms[i]));
    cJSON_AddItemToObject(o, "permissions", perms);

    cJSON_AddStringToObject(o, "last_ip", p->last_ip);
    cJSON_AddNumberToObject(o, "last_port", p->last_port);
    cJSON_AddNumberToObject(o, "last_seen_ms", (double)p->last_seen_ms);
    cJSON_AddNumberToObject(o, "created_at_ms", (double)p->created_at_ms);
    return o;
}

esp_err_t um_trust_ws_pairs_list(httpd_req_t *req, cJSON **out)
{
    (void)req;
    cJSON *arr = cJSON_CreateArray();
    if (!arr)
        return ESP_ERR_NO_MEM;

    if (s_lock && xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE)
    {
        for (int i = 0; i < s_peer_count; i++)
        {
            cJSON *o = peer_to_json(&s_peers[i]);
            if (o)
                cJSON_AddItemToArray(arr, o);
        }
        xSemaphoreGive(s_lock);
    }

    *out = arr;
    return ESP_OK;
}

esp_err_t um_trust_ws_pair_generate(httpd_req_t *req, cJSON *in, cJSON **out)
{
    (void)req;
    (void)in;

    char pair[128] = {0};
    esp_err_t e = um_trust_pair_generate(pair, sizeof(pair));
    if (e != ESP_OK)
        return e;

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "pair", pair);
    cJSON_AddNumberToObject(o, "expires_in", 300);
    *out = o;
    return ESP_OK;
}

esp_err_t um_trust_ws_pair_consume(httpd_req_t *req, cJSON *in, cJSON **out)
{
    (void)req;
    if (!cJSON_IsObject(in))
        return ESP_ERR_INVALID_ARG;

    cJSON *jpair = cJSON_GetObjectItem(in, "pair");
    if (!cJSON_IsString(jpair))
        return ESP_ERR_INVALID_ARG;

    char peer_id[UM_TRUST_ID_MAX] = {0};
    esp_err_t e = um_trust_pair_consume(jpair->valuestring, peer_id, sizeof(peer_id));
    if (e != ESP_OK)
        return e;

    /* Сообщаем peer'у о себе, чтобы он тоже добавил нас как доверенное */
    bool confirmed = false;
    char *resp = NULL;
    if (um_trust_call_peer(peer_id, "POST", "/api/trust/confirm", "{}", &resp) == ESP_OK)
        confirmed = true;
    free(resp);

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "peer", peer_id);
    cJSON_AddBoolToObject(o, "confirmed", confirmed);
    *out = o;
    return ESP_OK;
}

esp_err_t um_trust_ws_pair_confirm(httpd_req_t *req, cJSON *in, cJSON **out)
{
    (void)in;
    char dev[UM_TRUST_ID_MAX] = {0};
    httpd_req_get_hdr_value_str(req, "X-UMNI-Device", dev, sizeof(dev));

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "peer", dev);
    cJSON_AddBoolToObject(o, "paired", true);
    *out = o;
    return ESP_OK;
}

esp_err_t um_trust_ws_peer_update(httpd_req_t *req, cJSON *in, cJSON **out)
{
    (void)req;
    if (!cJSON_IsObject(in))
        return ESP_ERR_INVALID_ARG;

    cJSON *id = cJSON_GetObjectItem(in, "id");
    if (!cJSON_IsString(id))
        return ESP_ERR_INVALID_ARG;

    const um_trust_peer_t *peer = um_trust_get(id->valuestring);
    if (!peer)
        return ESP_ERR_NOT_FOUND;

    cJSON *jname = cJSON_GetObjectItem(in, "name");
    if (cJSON_IsString(jname))
    {
        um_trust_peer_t tmp = *peer;
        strlcpy(tmp.name, jname->valuestring, sizeof(tmp.name));
        um_trust_add_or_update(&tmp);
    }

    cJSON *jperms = cJSON_GetObjectItem(in, "permissions");
    if (cJSON_IsArray(jperms))
    {
        const char *perms[UM_TRUST_PERMS_MAX];
        int n = 0;
        cJSON *it = NULL;
        cJSON_ArrayForEach(it, jperms)
        {
            if (cJSON_IsString(it) && n < UM_TRUST_PERMS_MAX)
                perms[n++] = it->valuestring;
        }
        esp_err_t e = um_trust_set_perms(id->valuestring, perms, n);
        if (e != ESP_OK)
            return e;
    }

    *out = peer_to_json(um_trust_get(id->valuestring));
    return ESP_OK;
}

esp_err_t um_trust_ws_peer_delete(httpd_req_t *req, cJSON **out)
{
    char q[128] = {0};
    char id[UM_TRUST_ID_MAX] = {0};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK)
        httpd_query_key_value(q, "id", id, sizeof(id));

    if (!id[0])
        return ESP_ERR_INVALID_ARG;

    esp_err_t e = um_trust_remove(id);
    if (e != ESP_OK)
        return e;

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id", id);
    *out = o;
    return ESP_OK;
}

esp_err_t um_trust_ws_peer_state(httpd_req_t *req, cJSON *in, cJSON **out)
{
    (void)req;
    if (!cJSON_IsObject(in))
        return ESP_ERR_INVALID_ARG;

    cJSON *id = cJSON_GetObjectItem(in, "id");
    cJSON *cap = cJSON_GetObjectItem(in, "capability");
    if (!cJSON_IsString(id) || !cJSON_IsString(cap))
        return ESP_ERR_INVALID_ARG;

    cJSON *req_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(req_obj, "capability", cap->valuestring);
    char *req_body = cJSON_PrintUnformatted(req_obj);
    cJSON_Delete(req_obj);

    char *resp = NULL;
    esp_err_t e = um_trust_call_peer(id->valuestring, "POST", "/api/state", req_body, &resp);
    free(req_body);
    if (e != ESP_OK)
    {
        free(resp);
        return e;
    }

    cJSON *parsed = resp ? cJSON_Parse(resp) : NULL;
    free(resp);
    if (!parsed)
        return ESP_FAIL;

    *out = parsed;
    return ESP_OK;
}

esp_err_t um_trust_ws_peer_switch(httpd_req_t *req, cJSON *in, cJSON **out)
{
    (void)req;
    if (!cJSON_IsObject(in))
        return ESP_ERR_INVALID_ARG;

    cJSON *id = cJSON_GetObjectItem(in, "id");
    cJSON *mode = cJSON_GetObjectItem(in, "mode");
    cJSON *index = cJSON_GetObjectItem(in, "index");
    cJSON *level = cJSON_GetObjectItem(in, "level");
    if (!cJSON_IsString(id) || !cJSON_IsString(mode))
        return ESP_ERR_INVALID_ARG;

    cJSON *req_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(req_obj, "mode", mode->valuestring);
    if (cJSON_IsNumber(index))
        cJSON_AddNumberToObject(req_obj, "index", index->valueint);
    if (cJSON_IsBool(level))
        cJSON_AddBoolToObject(req_obj, "level", cJSON_IsTrue(level));
    char *req_body = cJSON_PrintUnformatted(req_obj);
    cJSON_Delete(req_obj);

    char *resp = NULL;
    esp_err_t e = um_trust_call_peer(id->valuestring, "POST", "/api/switch", req_body, &resp);
    free(req_body);
    if (e != ESP_OK)
    {
        free(resp);
        return e;
    }

    cJSON *parsed = resp ? cJSON_Parse(resp) : NULL;
    free(resp);
    if (!parsed)
    {
        *out = cJSON_CreateObject();
        return ESP_OK;
    }

    *out = parsed;
    return ESP_OK;
}