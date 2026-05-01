#include <string.h>
#include <sys/param.h>
#include "esp_log.h"

#include "base_config.h"
#include "um_nvs.h"
#include "um_store.h"
#include "um_helpers.h"
#include "um_webserver.h"
#include "um_capabilities.h"

#if UM_FEATURE_ENABLED(BUZZER)
#include "um_buzzer.h"
#endif

#if UM_FEATURE_ENABLED(ONEWIRE)
#include "um_onewire_config.h"
#endif

#if UM_FEATURE_ENABLED(OPENTHERM)
#include "um_opentherm.h"
#endif

#if UM_FEATURE_ENABLED(INPUTS) || UM_FEATURE_ENABLED(OUTPUTS)
#include "um_dio.h"
#include "um_dio_config.h"
#endif

#if UM_FEATURE_ENABLED(RF433)
#include "um_rf433_config.h"
#endif

#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
#include "um_adc_config.h"
#endif

#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2)
#include "um_ntc_config.h"
#endif

#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
#include "um_opencollectors.h"
#include "um_opencollectors_config.h"
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)

#define WEBSERVER_TAG "um_webserver"

static const char *REST_TAG = "um_webserver";
static httpd_handle_t server = NULL;

typedef esp_err_t (*um_data_provider_t)(httpd_req_t *req, cJSON **data_out);

typedef struct rest_server_context
{
    char base_path[UM_SD_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

typedef struct
{
    esp_err_t (*get_data)(httpd_req_t *, cJSON **);
} get_ctx_t;

typedef struct
{
    esp_err_t (*process_data)(httpd_req_t *, cJSON *, cJSON **);
} post_ctx_t;

// Простая HTML страница для теста, если нет SD карты
static const char *TEST_HTML =
    "<!DOCTYPE html><html><head><title>UM WebServer</title>"
    "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f5f5f5;}"
    ".container{max-width:800px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
    "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px;}"
    ".status{background:#e8f5e9;padding:15px;border-radius:5px;margin:20px 0;}"
    "</style></head>"
    "<body><div class='container'>"
    "<h1>UM WebServer</h1>"
    "<div class='status'>Веб-сервер работает успешно!</div>"
    "<p>Версия: 1.0.0</p>"
    "<p>Используйте REST API для взаимодействия</p>"
    "</div></body></html>";

/**
 *  Set HTTP response content type according to file extension
 *
 * @param   char       filepath  [filepath description]
 *
 * @return  esp_err_t            [return description]
 */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
    {
        type = "image/png";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
    {
        type = "image/x-icon";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
    {
        type = "image/svg+xml";
    }
    return httpd_resp_set_type(req, type);
}

static esp_err_t get_wrapper(httpd_req_t *req)
{
    get_ctx_t *ctx = (get_ctx_t *)req->user_ctx;
    return um_webserver_base_get_handler(req, ctx->get_data);
}

esp_err_t post_wrapper(httpd_req_t *req)
{
    post_ctx_t *ctx = (post_ctx_t *)req->user_ctx;
    return um_webserver_base_post_handler(req, ctx->process_data);
}

/**
 * Базовый обработчик для всех GET запросов
 * @param req HTTP запрос
 * @param get_data функция, которая заполняет data
 */
esp_err_t um_webserver_base_get_handler(
    httpd_req_t *req,
    esp_err_t (*get_data)(httpd_req_t *, cJSON **))
{
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_FAIL;
    }

    cJSON *data = NULL;
    esp_err_t ret = get_data(req, &data);

    cJSON_AddBoolToObject(root, "success", (ret == ESP_OK && data));

    if (ret == ESP_OK && data)
    {
        cJSON_AddItemToObject(root, "data", data);
    }
    else
    {
        const char *err_msg = "Unknown error";
        if (ret == ESP_ERR_INVALID_ARG)
            err_msg = "Invalid arguments";
        else if (ret == ESP_ERR_NOT_FOUND)
            err_msg = "Not found";
        else if (ret == ESP_ERR_NOT_SUPPORTED)
            err_msg = "Feature disabled";
        else if (ret == ESP_ERR_NO_MEM)
            err_msg = "Out of memory";

        cJSON_AddStringToObject(root, "error", err_msg);
    }

    char *response = cJSON_PrintUnformatted(root);
    esp_err_t http_ret = ESP_OK;

    if (response)
    {
        httpd_resp_sendstr(req, response);
        free(response);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
        http_ret = ESP_FAIL;
    }

    cJSON_Delete(root);
    ESP_LOGW(REST_TAG, "Free heap size before: %ld", esp_get_free_heap_size());
    return http_ret;
}

esp_err_t um_webserver_register_get(const char *uri, esp_err_t (*data_func)(httpd_req_t *, cJSON **))
{
    if (!server || !uri || !data_func)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Создаем контекст
    get_ctx_t *ctx = malloc(sizeof(get_ctx_t));
    if (!ctx)
    {
        return ESP_ERR_NO_MEM;
    }
    ctx->get_data = data_func;

    // Регистрируем
    httpd_uri_t uri_struct = {
        .uri = uri,
        .method = HTTP_GET,
        .handler = get_wrapper, // ← обертка, не базовый обработчик!
        .user_ctx = ctx,
        //.free_ctx = free            // ← просто free
    };

    return httpd_register_uri_handler(server, &uri_struct);
}

esp_err_t um_webserver_register_post(const char *uri,
                                     esp_err_t (*process_func)(httpd_req_t *, cJSON *, cJSON **))
{
    if (!server || !uri || !process_func)
        return ESP_ERR_INVALID_ARG;

    post_ctx_t *ctx = malloc(sizeof(post_ctx_t));
    if (!ctx)
        return ESP_ERR_NO_MEM;
    ctx->process_data = process_func;

    httpd_uri_t uri_struct = {
        .uri = uri,
        .method = HTTP_POST,
        .handler = post_wrapper,
        .user_ctx = ctx,
    };

    return httpd_register_uri_handler(server, &uri_struct);
}

/**
 * Базовый обработчик для ВСЕХ POST запросов
 * @param req HTTP запрос
 * @param process_data функция, которая обрабатывает входные данные и создает выходные
 */
esp_err_t um_webserver_base_post_handler(
    httpd_req_t *req,
    esp_err_t (*process_data)(httpd_req_t *, cJSON *input, cJSON **output))
{
    httpd_resp_set_type(req, "application/json");

    // 1. Читаем тело запроса (то, что прислал клиент)
    if (req->content_len == 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty request");
        return ESP_FAIL;
    }

    // Ограничим размер для безопасности
    if (req->content_len > 2048)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Request too large");
        return ESP_FAIL;
    }

    char *content = malloc(req->content_len + 1);
    if (!content)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_FAIL;
    }

    int received = httpd_req_recv(req, content, req->content_len);
    if (received <= 0)
    {
        free(content);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read data");
        return ESP_FAIL;
    }
    content[received] = '\0';

    // 2. Парсим входной JSON
    cJSON *input = cJSON_Parse(content);
    free(content);

    if (!input)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // 3. Вызываем функцию обработки
    cJSON *output = NULL;
    esp_err_t ret = process_data(req, input, &output);

    // 4. Формируем ответ (как в GET, но может включать данные от process_data)
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        cJSON_Delete(input);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(root, "success", (ret == ESP_OK));

    if (ret == ESP_OK)
    {
        if (output)
        {
            cJSON_AddItemToObject(root, "data", output);
        }
        else
        {
            cJSON_AddStringToObject(root, "message", "Operation successful");
        }
    }
    else
    {
        const char *err_msg = "Operation failed";
        if (ret == ESP_ERR_INVALID_ARG)
            err_msg = "Invalid arguments";
        else if (ret == ESP_ERR_NOT_FOUND)
            err_msg = "Resource not found";
        else if (ret == ESP_ERR_NOT_SUPPORTED)
            err_msg = "Feature disabled";
        else if (ret == ESP_ERR_NO_MEM)
            err_msg = "Out of memory";

        cJSON_AddStringToObject(root, "error", err_msg);
    }

    // 5. Отправляем ответ
    char *response = cJSON_PrintUnformatted(root);
    esp_err_t http_ret = ESP_OK;

    if (response)
    {
        httpd_resp_sendstr(req, response);
        free(response);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
        http_ret = ESP_FAIL;
    }

    cJSON_Delete(root);
    cJSON_Delete(input);

    ESP_LOGW(REST_TAG, "Free heap size before: %ld", esp_get_free_heap_size());

    return http_ret;
}

static esp_err_t get_systeminfo(httpd_req_t *req, cJSON **data)
{
    return um_helpers_get_systeminfo(data);
}

static esp_err_t get_config_data(httpd_req_t *req, cJSON **data)
{
    char section[32] = {0};

    // 1. Получаем параметры (всегда одинаково)
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0)
    {
        char *query = malloc(query_len + 1);
        if (!query)
            return ESP_ERR_NO_MEM;

        if (httpd_req_get_url_query_str(req, query, query_len + 1) == ESP_OK)
        {
            httpd_query_key_value(query, "section", section, sizeof(section));
        }
        free(query);
    }

    // 2. Проверяем обязательные параметры
    if (strlen(section) == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // 3. Твоя логика получения данных
    char *config_str = NULL;

    if (strcmp(section, "onewire") == 0)
    {
#if UM_FEATURE_ENABLED(ONEWIRE)
        config_str = um_onewire_config_read();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (strcmp(section, "inputs") == 0)
    {
#if UM_FEATURE_ENABLED(INPUTS)
        config_str = um_dio_config_get_inputs_json();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (strcmp(section, "outputs") == 0)
    {
#if UM_FEATURE_ENABLED(INPUTS)
        config_str = um_dio_config_get_outputs_json();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (strcmp(section, "opencollectors") == 0)
    {
#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
        config_str = um_oc_config_get_json();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (strcmp(section, "rf433") == 0)
    {
#if UM_FEATURE_ENABLED(RF433)
        config_str = um_rf433_config_read();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (strcmp(section, "adc") == 0)
    {
#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
        config_str = um_adc_config_read();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else if (strcmp(section, "ntc") == 0)
    {
#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2)
        config_str = um_ntc_config_read();
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    }
    else
    {
        return ESP_ERR_NOT_FOUND;
    }

    if (!config_str)
    {
        return ESP_FAIL;
    }

    // 4. Парсим JSON (всегда одинаково для строк)
    cJSON *json = cJSON_Parse(config_str);
    free(config_str);

    if (!json)
    {
        return ESP_FAIL;
    }

    *data = json;
    return ESP_OK;
}

static esp_err_t um_webserver_on_off_handler(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *mode = cJSON_GetObjectItem(input, "mode");
    cJSON *level = cJSON_GetObjectItem(input, "level");
    cJSON *index = cJSON_GetObjectItem(input, "index");

    // 2. Проверяем обязательные параметры
    if (cJSON_IsString(mode) && strlen(mode->valuestring) == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(mode->valuestring, "outputs") == 0)
    {
#if UM_FEATURE_ENABLED(OUTPUTS)
        if (cJSON_IsBool(level) && cJSON_IsNumber(index) && index->valueint >= 0)
        {
            int idx = index->valueint;
            int lvl = cJSON_IsTrue(level) ? 1 : 0;

            ESP_LOGI(REST_TAG, "Setting output %d to %d", idx, lvl);

            if (idx > 0 && idx <= 8 && (lvl == 0 || lvl == 1))
            {
                um_dio_set_output(idx - 1, lvl); // fix
            }
            else
            {
                ESP_LOGW(REST_TAG, "Invalid values: index=%d, level=%d", idx, lvl);
                return ESP_ERR_INVALID_ARG;
            }
        }
#endif
    }
    else if (strcmp(mode->valuestring, "opencollectors") == 0)
    {
#if UM_FEATURE_ENABLED(OPENCOLLECTORS)
        if (cJSON_IsBool(level) && cJSON_IsNumber(index) && index->valueint >= 0)
        {
            int lvl = cJSON_IsTrue(level) ? 1 : 0;
            return um_opencollectors_set(
                index->valueint,
                lvl);
        }
#endif
    }
    else
    {
        return ESP_ERR_NOT_FOUND;
    }

    *output = NULL;
    return ESP_OK;
}

static esp_err_t um_webserver_beep_handler(httpd_req_t *req, cJSON *input, cJSON **output)
{

#if UM_FEATURE_ENABLED(BUZZER)
    cJSON *beep_count = cJSON_GetObjectItem(input, "count");
    cJSON *beep_on_ms = cJSON_GetObjectItem(input, "on_ms");
    cJSON *beep_off_ms = cJSON_GetObjectItem(input, "off_ms");
    if (
        cJSON_IsNumber(beep_count) &&
        cJSON_IsNumber(beep_on_ms) &&
        cJSON_IsNumber(beep_off_ms))
    {
        uint8_t beep_count_res = beep_count->valueint > 16 || beep_count->valueint < 0 ? 3 : beep_count->valueint;
        uint16_t beep_on_ms_res = beep_on_ms->valueint > 1000 || beep_on_ms->valueint < 0 ? 100 : beep_on_ms->valueint;
        uint16_t beep_off_ms_res = beep_off_ms->valueint > 1000 || beep_off_ms->valueint < 0 ? 100 : beep_off_ms->valueint;
        um_buzzer_beep_in_task(beep_count_res, beep_on_ms_res, beep_off_ms_res);
    }

#endif
    *output = NULL;
    return ESP_OK;
}

static esp_err_t um_webserver_state_handler(httpd_req_t *req, cJSON *input, cJSON **output)
{
    cJSON *capability = cJSON_GetObjectItem(input, "capability");
    cJSON *result = NULL;
    cJSON *history = NULL;
    cJSON *data = NULL;
    if (cJSON_IsString(capability))
    {
        // получение настроек или значений согласно capability
        if (um_capabilities_has_by_name(capability->valuestring))
        {

            um_store_t *history_store = um_store_find_store(capability->valuestring);
            // функция доступна
            um_capability_t cap = um_capabilities_get_by_name(capability->valuestring);
            result = cJSON_CreateObject();
            if (cap == UM_CAP_OPENTHERM)
            {
                //
                char *ot_json = um_ot_get_status_json();
                data = cJSON_Parse(ot_json);
                free(ot_json);
            }
            else if (cap == UM_CAP_NTC1)
            {
                data = cJSON_CreateObject();
                float temp1 = 0;
                um_ntc_get_last_temperature(UM_NTC_CHANNEL_1, &temp1);
                cJSON_AddNumberToObject(data, "value", temp1);
            }
            else if (cap == UM_CAP_NTC2)
            {
                data = cJSON_CreateObject();
                float temp2 = 0;
                um_ntc_get_last_temperature(UM_NTC_CHANNEL_2, &temp2);
                cJSON_AddNumberToObject(data, "value", temp2);
            }
            else if (cap == UM_CAP_AI1)
            {
                data = cJSON_CreateObject();
                int ai1 = 0;
                um_adc_get_last_raw(UM_ADC_CHANNEL_1, &ai1);
                cJSON_AddNumberToObject(data, "value", ai1);
            }
            else if (cap == UM_CAP_AI2)
            {
                data = cJSON_CreateObject();
                int ai2 = 0;
                um_adc_get_last_raw(UM_ADC_CHANNEL_2, &ai2);
                cJSON_AddNumberToObject(data, "value", ai2);
            }

            cJSON_AddItemToObject(result, "state", data);

            if (history_store != NULL)
            {
                char *json_history = um_store_to_json(history_store);

                history = cJSON_Parse(json_history);
                cJSON_AddItemToObject(result, "history", history);

                free(json_history);
            }

            *output = result;
            return ESP_OK;
        }
    }

    *output = result;
    return ESP_ERR_INVALID_ARG;
}

/**
 * @brief Обработчик для входа (POST)
 */
static esp_err_t um_webserver_login_handler(httpd_req_t *req, cJSON *input, cJSON **output)
{
    // Извлекаем данные из запроса
    cJSON *username = cJSON_GetObjectItem(input, "username");
    cJSON *password = cJSON_GetObjectItem(input, "password");

    if (!username || !password || !username->valuestring || !password->valuestring)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Проверка (в реальности - из NVS)
    if (strcmp(username->valuestring, "admin") == 0 &&
        strcmp(password->valuestring, "1234") == 0)
    {

        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "token", "secret_token_12345");
        cJSON_AddNumberToObject(data, "expires_in", 3600);

        *output = data;
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND; // Неверные учетные данные
}

static esp_err_t um_webserver_save_settings_handler(httpd_req_t *req, cJSON *input, cJSON **output)
{
    // Ключ настройки
    cJSON *setting = cJSON_GetObjectItem(input, "setting");

    // Объект значений настройки
    cJSON *values = cJSON_GetObjectItem(input, "values");

    if (!setting || !setting->valuestring || !values || !cJSON_IsObject(values))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(setting->valuestring, "mqtt") == 0)
    {
#if UM_FEATURE_ENABLED(MQTT)
        // {values: {en: bool, host: string, port: int, user: ?string, password: ?string}}
        cJSON *en = cJSON_GetObjectItem(values, "en");
        cJSON *host = cJSON_GetObjectItem(values, "host");
        cJSON *port = cJSON_GetObjectItem(values, "port");
        cJSON *username = cJSON_GetObjectItem(values, "username");
        cJSON *password = cJSON_GetObjectItem(values, "password");
        bool auth = false;
        if (username && cJSON_IsString(username) && password && cJSON_IsString(password))
        {
            auth = true;
        }
        if (en && cJSON_IsBool(en))
        {
            um_nvs_set_mqtt_enabled(cJSON_IsTrue(en));
        }
        um_nvs_set_mqtt_username(auth ? username->valuestring : NULL);
        um_nvs_set_mqtt_password(auth ? password->valuestring : NULL);

        um_nvs_set_mqtt_host(host && cJSON_IsString(host) ? host->valuestring : "localhost");
        um_nvs_set_mqtt_port(port && cJSON_IsNumber(port) ? port->valueint : 1883);
        cJSON *data = cJSON_CreateObject();

        *output = data;
#endif
        return ESP_OK;
    }
    else if (strcmp(setting->valuestring, "webhook") == 0)
    {
#if UM_FEATURE_ENABLED(WEBHOOKS)
        cJSON *whk_en = cJSON_GetObjectItem(values, "en");
        cJSON *whk_url = cJSON_GetObjectItem(values, "url");
        if (cJSON_IsBool(whk_en))
        {
            um_nvs_set_webhooks_enabled(cJSON_IsTrue(whk_en));
        }
        if (cJSON_IsString(whk_url))
        {
            um_nvs_set_webhooks_url(whk_url->valuestring);
        }
#endif
    }
    else if (strcmp(setting->valuestring, "outputs") == 0)
    {
#if UM_FEATURE_ENABLED(OUTPUTS)
        cJSON *do_index = cJSON_GetObjectItem(values, "index");
        cJSON *do_en = cJSON_GetObjectItem(values, "en");
        cJSON *do_label = cJSON_GetObjectItem(values, "label");

        if (
            cJSON_IsNumber(do_index) &&
            cJSON_IsBool(do_en) &&
            cJSON_IsString(do_label))
        {
            esp_err_t res = ESP_FAIL;
            bool default_state = false;
            res = um_dio_get_output(do_index->valueint, &default_state);

            if (res != ESP_OK)
                return res;

            res = um_dio_config_update_output(
                do_index->valueint,
                do_label->valuestring,
                cJSON_IsTrue(do_en),
                default_state);

            if (res != ESP_OK)
                return res;

            return um_dio_config_save();
        }
        else
        {
            return ESP_FAIL;
        }
#endif
    }
    else if (strcmp(setting->valuestring, "inputs") == 0)
    {
#if UM_FEATURE_ENABLED(INPUTS)
        cJSON *di_index = cJSON_GetObjectItem(values, "index");
        cJSON *di_en = cJSON_GetObjectItem(values, "en");
        cJSON *di_label = cJSON_GetObjectItem(values, "label");

        if (
            cJSON_IsNumber(di_index) &&
            cJSON_IsBool(di_en) &&
            cJSON_IsString(di_label))
        {
            esp_err_t res = ESP_FAIL;
            res = um_dio_config_update_input(
                di_index->valueint,
                di_label->valuestring,
                cJSON_IsTrue(di_en));

            if (res != ESP_OK)
                return res;

            return um_dio_config_save();
        }
        else
        {
            return ESP_FAIL;
        }
#endif
    }
    else if (strcmp(setting->valuestring, "ntc") == 0)
    {
#if UM_FEATURE_ENABLED(NTC1) || UM_FEATURE_ENABLED(NTC2)
        cJSON *ntc_channel = cJSON_GetObjectItem(values, "channel");
        cJSON *ntc_active = cJSON_GetObjectItem(values, "active");
        cJSON *ntc_calibration_offset = cJSON_GetObjectItem(values, "offset");
        cJSON *ntc_label = cJSON_GetObjectItem(values, "label");
        if (
            cJSON_IsNumber(ntc_channel) &&
            cJSON_IsBool(ntc_active) &&
            cJSON_IsNumber(ntc_calibration_offset) &&
            cJSON_IsString(ntc_label))
        {

            um_ntc_channel_config_t ntc_config = {0}; // Сначала обнуляем
            ntc_config.channel_id = ntc_channel->valueint;
            strlcpy(ntc_config.label, ntc_label->valuestring, sizeof(ntc_config.label));
            ntc_config.active = cJSON_IsTrue(ntc_active);
            ntc_config.calibration_offset = ntc_calibration_offset->valuedouble;
            um_ntc_config_update(
                ntc_channel->valueint,
                &ntc_config);
            return um_ntc_config_save();
        }
        else
        {
            return ESP_ERR_INVALID_ARG;
        }
#endif
    }
    else if (strcmp(setting->valuestring, "adc") == 0)
    {
#if UM_FEATURE_ENABLED(AI1) || UM_FEATURE_ENABLED(AI2)
        cJSON *adc_channel = cJSON_GetObjectItem(values, "channel");
        cJSON *adc_active = cJSON_GetObjectItem(values, "active");
        cJSON *adc_offset = cJSON_GetObjectItem(values, "offset");
        cJSON *adc_label = cJSON_GetObjectItem(values, "label");
        if (
            cJSON_IsNumber(adc_channel) &&
            cJSON_IsBool(adc_active) &&
            cJSON_IsNumber(adc_offset) &&
            cJSON_IsString(adc_label))
        {

            um_adc_channel_config_t adc_config = {0}; // Сначала обнуляем
            adc_config.channel_id = adc_channel->valueint;
            strlcpy(adc_config.label, adc_label->valuestring, sizeof(adc_config.label));
            adc_config.active = cJSON_IsTrue(adc_active);
            adc_config.offset = adc_offset->valuedouble;
            um_adc_config_update(
                adc_channel->valueint,
                &adc_config);
            return um_adc_config_save();
        }
        else
        {
            return ESP_ERR_INVALID_ARG;
        }
#endif
    }
    else if (strcmp(setting->valuestring, "opentherm") == 0)
    {
#if UM_FEATURE_ENABLED(OPENTHERM)
        cJSON *ot_en = cJSON_GetObjectItem(values, "en");

        cJSON *ot_ch_en = cJSON_GetObjectItem(values, "ch_en");
        cJSON *ot_ch_sp = cJSON_GetObjectItem(values, "ch_sp");

        cJSON *ot_dhw_en = cJSON_GetObjectItem(values, "dhw_en");
        cJSON *ot_dhw_sp = cJSON_GetObjectItem(values, "dhw_sp");

        cJSON *ot_ch2_en = cJSON_GetObjectItem(values, "ch2_en");

        cJSON *ot_cool_en = cJSON_GetObjectItem(values, "cool_en");

        cJSON *ot_mod = cJSON_GetObjectItem(values, "mod");

        // TODO cJSON *ot_hcr = cJSON_GetObjectItem(values, "hcr");

        cJSON *ot_otc_en = cJSON_GetObjectItem(values, "otc_en");

        if (cJSON_IsBool(ot_en))
        {
            um_nvs_set_ot_enabled(cJSON_IsTrue(ot_en));
        }
        if (cJSON_IsBool(ot_ch_en))
        {
            um_nvs_set_ot_ch_enabled(cJSON_IsTrue(ot_ch_en));
        }
        if (cJSON_IsNumber(ot_ch_sp))
        {
            um_nvs_set_ot_ch_setpoint((uint8_t)ot_ch_sp->valueint);
        }
        if (cJSON_IsBool(ot_dhw_en))
        {
            um_nvs_set_ot_dhw_enabled(cJSON_IsTrue(ot_dhw_en));
        }
        if (cJSON_IsNumber(ot_dhw_sp))
        {
            um_nvs_set_ot_dhw_setpoint((uint8_t)ot_dhw_sp->valueint);
        }
        if (cJSON_IsBool(ot_ch2_en))
        {
            um_nvs_set_ot_ch2_enabled(cJSON_IsTrue(ot_ch2_en));
        }
        if (cJSON_IsBool(ot_cool_en))
        {
            um_nvs_set_ot_cool_enabled(cJSON_IsTrue(ot_cool_en));
        }
        if (cJSON_IsNumber(ot_mod))
        {
            um_nvs_set_ot_modulation((uint8_t)ot_mod->valueint);
        }
        // TODO um_nvs_set_ot_heating_curve_ratio();
        if (cJSON_IsBool(ot_otc_en))
        {
            um_nvs_set_ot_outdoor_temp_comp(cJSON_IsTrue(ot_otc_en));
        }
#endif
    }
    return ESP_OK;
}

/**
 * @brief Обработчик для статических файлов
 */
static esp_err_t um_webserver_static_handler(httpd_req_t *req)
{
    ESP_LOGI(REST_TAG, "Statix file query: %s", req->uri);

#if UM_FEATURE_ENABLED(SDCARD)
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;

    strlcpy(filepath, rest_context->base_path, sizeof(filepath));

    if (req->uri[strlen(req->uri) - 1] == '/')
    {
        strlcat(filepath, "/index.html", sizeof(filepath));
    }
    else
    {
        strlcat(filepath, req->uri, sizeof(filepath));
        char *token = strtok(filepath, "?");
        if (token != NULL)
        {
            // printf(" %s\n", token);
        }
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1)
    {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do
    {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1)
        {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        }
        else if (read_bytes > 0)
        {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
            {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    // ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
#else
    // Возвращаем тестовую HTML страницу
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, TEST_HTML);
#endif

    return ESP_OK;
}

/**
 * @brief Инициализация веб-сервера
 */
esp_err_t um_webserver_start(void)
{
    ESP_LOGI(WEBSERVER_TAG, "Starting web-server");

    // Конфигурация сервера
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    char *base_path = CONFIG_UMNI_SD_MOUNT_POINT "/www";

    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    config.max_open_sockets = 7;      // Максимум открытых сокетов (по умолчанию 7)
    config.lru_purge_enable = true;   // Включить LRU очистку старых соединений
    config.recv_wait_timeout = 5;     // Таймаут приема (сек)
    config.send_wait_timeout = 5;     // Таймаут отправки (сек)
    config.keep_alive_enable = false; // Отключить Keep-Alive (освобождает сокеты)

    // Запуск сервера
    esp_err_t ret = httpd_start(&server, &config);
    REST_CHECK(ret == ESP_OK, "Start server failed", err_start);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WEBSERVER_TAG, "Web-server start error: %s", esp_err_to_name(ret));
        return ret;
    }

    um_webserver_register_get("/api/systeminfo", get_systeminfo);
    um_webserver_register_get("/api/conf", get_config_data);
    um_webserver_register_post("/api/switch", um_webserver_on_off_handler);
    um_webserver_register_post("/api/login", um_webserver_login_handler);
    um_webserver_register_post("/api/beep", um_webserver_beep_handler);
    um_webserver_register_post("/api/settings", um_webserver_save_settings_handler);
    um_webserver_register_post("/api/state", um_webserver_state_handler);

    // Обработчик для корневого пути (статический HTML)
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = um_webserver_static_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &root_uri);

    // Обработчик для index.html
    httpd_uri_t index_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = um_webserver_static_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &index_uri);

    ESP_LOGI(WEBSERVER_TAG, "Web-server started successfully");
    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}

/**
 * @brief Остановка веб-сервера
 */
esp_err_t um_webserver_stop(void)
{
    if (server)
    {
        ESP_LOGI(WEBSERVER_TAG, "Stopping web-server");
        httpd_stop(server);
        server = NULL;
    }
    return ESP_OK;
}

#else // UM_FEATURE_ENABLED(WEBSERVER)

// Заглушки, если фича отключена
esp_err_t um_webserver_start(void) { return ESP_OK; }
esp_err_t um_webserver_stop(void) { return ESP_OK; }

#endif // UM_FEATURE_ENABLED(WEBSERVER)