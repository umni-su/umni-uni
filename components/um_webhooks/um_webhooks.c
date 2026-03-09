#include "um_webhooks.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "UM_WEBHOOKS";

bool um_webhooks_is_enabled(void)
{
    bool enabled = false;
    esp_err_t err = um_nvs_get_webhooks_enabled(&enabled);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get webhooks enabled status: %s", esp_err_to_name(err));
        return false;
    }

    return enabled;
}

char *um_webhooks_get_url(void)
{
    char *url = NULL;
    esp_err_t err = um_nvs_get_webhooks_url(&url);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get webhooks URL: %s", esp_err_to_name(err));
        return NULL;
    }

    return url;
}

static esp_err_t um_webhooks_send_request(const char *url, const char *method,
                                          const char *data, int timeout_ms)
{
    if (url == NULL)
    {
        ESP_LOGE(TAG, "URL is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    bool is_https = (strncmp(url, "https://", 8) == 0);

    esp_http_client_config_t config = {
        .url = url,
        .method = strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = timeout_ms > 0 ? timeout_ms : 10000, // Использовать переданный таймаут или 10с
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .skip_cert_common_name_check = true,
        .keep_alive_enable = false,
    };

    // Для HTTPS добавляем настройки TLS
    if (is_https)
    {
        ESP_LOGI(TAG, "Using HTTPS for URL: %s", url);
        config.cert_pem = NULL; // Используем стандартные корневые сертификаты
        config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    // Добавляем заголовки
    esp_http_client_set_header(client, "User-Agent", "ESP32-Webhook/1.0");
    esp_http_client_set_header(client, "Accept", "application/json");

    if (data != NULL && strcmp(method, "POST") == 0)
    {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, data, strlen(data));
        ESP_LOGD(TAG, "Sending data: %s", data);
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        int64_t content_length = esp_http_client_get_content_length(client);

        ESP_LOGI(TAG, "HTTP %s Status = %d, Content-Length = %lld, URL: %s",
                 method, status, content_length, url);

        if (status < 200 || status >= 300)
        {
            ESP_LOGW(TAG, "HTTP status not success: %d", status);
            err = ESP_FAIL;
        }

        // Читаем ответ для отладки (опционально)
        char buffer[256];
        int read_len = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
        if (read_len > 0)
        {
            buffer[read_len] = '\0';
            ESP_LOGD(TAG, "Response: %s", buffer);
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP %s failed: %s", method, esp_err_to_name(err));
        if (is_https)
        {
            ESP_LOGE(TAG, "HTTPS connection failed. Check: certificate, network, URL");
        }
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t um_webhooks_post_string(const char *json_string)
{
    // Используем таймаут по умолчанию (10 секунд)
    return um_webhooks_post_string_timeout(json_string, 0);
}

esp_err_t um_webhooks_post_string_timeout(const char *json_string, int timeout_ms)
{
    if (!um_webhooks_is_enabled())
    {
        ESP_LOGI(TAG, "Webhooks are disabled, skipping POST");
        return ESP_OK;
    }

    char *url = um_webhooks_get_url();
    if (url == NULL)
    {
        ESP_LOGE(TAG, "No webhook URL configured");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Sending POST to webhook: %s", url);
    esp_err_t err = um_webhooks_send_request(url, "POST", json_string, timeout_ms);

    free(url);
    return err;
}

esp_err_t um_webhooks_post_json(cJSON *json)
{
    return um_webhooks_post_json_timeout(json, 0);
}

esp_err_t um_webhooks_post_json_timeout(cJSON *json, int timeout_ms)
{
    if (json == NULL)
    {
        return um_webhooks_post_string_timeout("{}", timeout_ms);
    }

    char *json_string = cJSON_PrintUnformatted(json);
    if (json_string == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = um_webhooks_post_string_timeout(json_string, timeout_ms);
    free(json_string);

    return err;
}

esp_err_t um_webhooks_get(const char *query_string)
{
    return um_webhooks_get_timeout(query_string, 0);
}

esp_err_t um_webhooks_get_timeout(const char *query_string, int timeout_ms)
{
    if (!um_webhooks_is_enabled())
    {
        ESP_LOGI(TAG, "Webhooks are disabled, skipping GET");
        return ESP_OK;
    }

    char *url = um_webhooks_get_url();
    if (url == NULL)
    {
        ESP_LOGE(TAG, "No webhook URL configured");
        return ESP_ERR_INVALID_STATE;
    }

    char *full_url = NULL;

    if (query_string != NULL && strlen(query_string) > 0)
    {
        int url_len = strlen(url) + strlen(query_string) + 1;
        full_url = malloc(url_len);

        if (full_url != NULL)
        {
            strcpy(full_url, url);
            strcat(full_url, query_string);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate memory for full URL");
            free(url);
            return ESP_ERR_NO_MEM;
        }
    }
    else
    {
        full_url = strdup(url);
    }

    ESP_LOGI(TAG, "Sending GET to webhook: %s", full_url);
    esp_err_t err = um_webhooks_send_request(full_url, "GET", NULL, timeout_ms);

    free(url);
    free(full_url);
    return err;
}