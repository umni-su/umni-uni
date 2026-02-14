#include <string.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "base_config.h"

#include "um_webserver.h"

#if UM_FEATURE_ENABLED(ONEWIRE)
#include "um_onewire_config.h"
#endif

#if UM_FEATURE_ENABLED(WEBSERVER)

#define WEBSERVER_TAG "um_webserver"

static const char *REST_TAG = "um_webserver";
static httpd_handle_t server = NULL;

typedef esp_err_t (*um_data_provider_t)(httpd_req_t *req, cJSON **data_out);

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
 * Базовый обработчик для всех GET запросов
 * @param req HTTP запрос
 * @param get_data функция, которая заполняет data
 */
static esp_err_t um_webserver_base_get_handler(
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
    return http_ret;
}

/**
 * Базовый обработчик для ВСЕХ POST запросов
 * @param req HTTP запрос
 * @param process_data функция, которая обрабатывает входные данные и создает выходные
 */
static esp_err_t um_webserver_base_post_handler(
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

    return http_ret;
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

static esp_err_t um_webserver_get_config_handler(httpd_req_t *req)
{
    return um_webserver_base_get_handler(req, get_config_data);
}

/**
 * @brief Тестовый GET обработчик
 */
static esp_err_t um_webserver_test_get_handler(httpd_req_t *req)
{
    ESP_LOGI(REST_TAG, "GET запрос на URI: %s", req->uri);

    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Ошибка создания JSON");
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(response, "status", "success");
    cJSON_AddStringToObject(response, "message", "Веб-сервер работает");
    cJSON_AddNumberToObject(response, "timestamp", 124);
    cJSON_AddStringToObject(response, "uri", req->uri);

    const char *json_str = cJSON_PrintUnformatted(response);
    if (!json_str)
    {
        cJSON_Delete(response);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Ошибка формирования JSON");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free((void *)json_str);
    cJSON_Delete(response);

    return ESP_OK;
}

/**
 * @brief Обработчик для входа (POST)
 */
static esp_err_t um_webserver_login_handler(httpd_req_t *req)
{
    ESP_LOGI(REST_TAG, "POST запрос на URI: %s", req->uri);

    // Проверяем размер данных
    if (req->content_len > 1024)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Слишком большой запрос");
        return ESP_FAIL;
    }

    // Читаем данные
    char buf[1025];
    int received = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    if (received <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Ошибка чтения данных");
        return ESP_FAIL;
    }
    buf[received] = '\0';

    // Парсим JSON
    cJSON *json = cJSON_Parse(buf);
    if (!json)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Неверный JSON");
        return ESP_FAIL;
    }

    // Извлекаем данные
    cJSON *username = cJSON_GetObjectItem(json, "username");
    cJSON *password = cJSON_GetObjectItem(json, "password");

    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Ошибка создания ответа");
        return ESP_FAIL;
    }

    // Простая проверка (в реальном коде здесь будет проверка с NVS)
    if (username && password &&
        username->valuestring && password->valuestring &&
        strcmp(username->valuestring, "admin") == 0)
    {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Вход выполнен");
        cJSON_AddStringToObject(response, "token", "dummy_token_12345");
    }
    else
    {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Неверные учетные данные");
    }

    const char *response_str = cJSON_PrintUnformatted(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response_str);

    free((void *)response_str);
    cJSON_Delete(response);
    cJSON_Delete(json);

    return ESP_OK;
}

/**
 * @brief Обработчик для статических файлов
 */
static esp_err_t um_webserver_static_handler(httpd_req_t *req)
{
    ESP_LOGI(REST_TAG, "Statix file query: %s", req->uri);

#if UM_FEATURE_ENABLED(SDCARD)
    // Здесь будет код для чтения файлов с SD карты
    // Пока просто возвращаем тестовую страницу
#endif

    // Возвращаем тестовую HTML страницу
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, TEST_HTML);

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
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.stack_size = 8192;

    // Запуск сервера
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WEBSERVER_TAG, "Web-server start error: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t config_get_uri = {
        .uri = "/api/conf",
        .method = HTTP_GET,
        .handler = um_webserver_get_config_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &config_get_uri);

    // Регистрация обработчиков

    // Тестовый GET метод
    httpd_uri_t test_get_uri = {
        .uri = "/api/test",
        .method = HTTP_GET,
        .handler = um_webserver_test_get_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &test_get_uri);

    // POST метод для входа
    httpd_uri_t login_post_uri = {
        .uri = "/api/login",
        .method = HTTP_POST,
        .handler = um_webserver_login_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &login_post_uri);

    // Обработчик для корневого пути (статический HTML)
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = um_webserver_static_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &root_uri);

    // Обработчик для index.html
    httpd_uri_t index_uri = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = um_webserver_static_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &index_uri);

    ESP_LOGI(WEBSERVER_TAG, "Web-server started successfully");
    return ESP_OK;
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