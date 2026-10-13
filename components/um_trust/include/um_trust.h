#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "cJSON.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UM_TRUST_MAX_PEERS 8
#define UM_TRUST_SECRET_LEN 32
#define UM_TRUST_ID_MAX 32
#define UM_TRUST_NAME_MAX 64
#define UM_TRUST_PERMS_MAX 16
#define UM_TRUST_PERM_LEN_MAX 24
#define UM_TRUST_IP_LEN 16
#define UM_TRUST_SIG_HEX_LEN (32 * 2 + 1)
#define UM_TRUST_NONCE_LEN 16
#define UM_TRUST_TS_WINDOW_MS 60000LL
#define UM_TRUST_IP_TTL_MS (5LL * 60LL * 1000LL) // 5 минут
#define UM_TRUST_CONFIG_PATH "/spiffs/trust.json"
#define UM_TRUST_CONFIG_VERSION 1

    typedef struct
    {
        char id[UM_TRUST_ID_MAX];
        char name[UM_TRUST_NAME_MAX];
        uint8_t secret[UM_TRUST_SECRET_LEN];
        char perms[UM_TRUST_PERMS_MAX][UM_TRUST_PERM_LEN_MAX];
        uint8_t perms_count;
        char last_ip[UM_TRUST_IP_LEN];
        uint16_t last_port;
        int64_t last_seen_ms;
        int64_t created_at_ms;
    } um_trust_peer_t;

    typedef enum
    {
        UM_AUTH_NONE = 0,
        UM_AUTH_LOCAL,
        UM_AUTH_PEER,
    } um_auth_kind_t;

    typedef struct
    {
        um_auth_kind_t kind;
        char peer_id[UM_TRUST_ID_MAX];
    } um_auth_ctx_t;

    /* === Lifecycle === */
    esp_err_t um_trust_init(void);

    /* === Peers === */
    const um_trust_peer_t *um_trust_get(const char *peer_id);
    int um_trust_list(const um_trust_peer_t **out, int max);
    esp_err_t um_trust_add_or_update(const um_trust_peer_t *peer);
    esp_err_t um_trust_remove(const char *peer_id);
    esp_err_t um_trust_set_perms(const char *peer_id,
                                 const char *const *perms, int n);
    bool um_trust_peer_has(const char *peer_id, const char *cap);
    esp_err_t um_trust_update_ip(const char *peer_id,
                                 const char *ip, uint16_t port);
    /* Принудительно сбросить конфиг на диск (например, при shutdown) */
    esp_err_t um_trust_flush(void);

    /* === Pairing (PSK, base32) === */
    esp_err_t um_trust_pair_generate(char *out_str, size_t out_len);
    esp_err_t um_trust_pair_consume(const char *pair_str,
                                    char *out_peer_id, size_t out_peer_id_len);

    /**
     * @brief Подтвердить сопряжение по «висящему» секрету.
     *
     * Вызывается на стороне, сгенерировавшей код. Проверяет HMAC входящего
     * запроса по всем неистёкшим одноразовым секретам, и при совпадении
     * регистрирует отправителя как peer с этим общим секретом.
     *
     * @param req HTTP-запрос (заголовки X-UMNI-*)
     * @param body Тело запроса
     * @param body_len Длина тела
     * @param[out] ctx Контекст авторизации (kind=PEER при успехе)
     */
    esp_err_t um_trust_pair_confirm(httpd_req_t *req,
                                    const uint8_t *body, size_t body_len,
                                    um_auth_ctx_t *ctx);

    /* === HMAC === */
    esp_err_t um_trust_sign(const um_trust_peer_t *peer,
                            const char *method, const char *path,
                            const uint8_t *body, size_t body_len,
                            char *out_sig, size_t sig_len,
                            char *out_ts, size_t ts_len,
                            char *out_nonce, size_t nonce_len);

    esp_err_t um_trust_verify_body(httpd_req_t *req,
                                   const uint8_t *body, size_t body_len,
                                   um_auth_ctx_t *ctx);

    /* === Enforcement === */
    bool um_trust_enforce(const um_auth_ctx_t *ctx, httpd_req_t *req, cJSON *input);

    /* === HTTP-клиент к peer'у === */
    esp_err_t um_trust_call_peer(const char *peer_id,
                                 const char *method,
                                 const char *path,
                                 const char *body,
                                 char **resp_out);

    /* === Web-server handlers === */
    esp_err_t um_trust_ws_pairs_list(httpd_req_t *req, cJSON **out);
    esp_err_t um_trust_ws_pair_generate(httpd_req_t *req, cJSON *in, cJSON **out);
    esp_err_t um_trust_ws_pair_consume(httpd_req_t *req, cJSON *in, cJSON **out);
    esp_err_t um_trust_ws_pair_confirm(httpd_req_t *req, cJSON *in, cJSON **out);
    esp_err_t um_trust_ws_peer_delete(httpd_req_t *req, cJSON **out);
    esp_err_t um_trust_ws_peer_update(httpd_req_t *req, cJSON *in, cJSON **out);
    esp_err_t um_trust_ws_peer_state(httpd_req_t *req, cJSON *in, cJSON **out);
    esp_err_t um_trust_ws_peer_switch(httpd_req_t *req, cJSON *in, cJSON **out);

#ifdef __cplusplus
}
#endif