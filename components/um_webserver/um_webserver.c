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

static esp_err_t um_webserver_get_config_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    char *config = NULL;

    char section[32] = {0};

    // Получаем параметр "section" из query string
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len > 0)
    {
        char *query = malloc(query_len + 1);
        httpd_req_get_url_query_str(req, query, query_len + 1);

        httpd_query_key_value(query, "section", section, sizeof(section));
        free(query);
    }

    if (strcmp(section, "onewire") == 0)
    {
#if UM_FEATURE_ENABLED(ONEWIRE)
        config = um_onewire_config_read();
#endif
    }

    cJSON *root = cJSON_CreateObject();
    bool success = false;

    if (config != NULL)
    {
        cJSON *data = cJSON_Parse(config);
        if (cJSON_IsObject(root))
        {
            success = true;
            cJSON_AddItemToObject(root, "data", data);
        }
    }
    cJSON_AddBoolToObject(root, "success", success);
    char *response = cJSON_PrintUnformatted(root);
    if (response)
    {
        httpd_resp_sendstr(req, response);
        free(response);
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
    }
    cJSON_Delete(root);
    free(config);

    ESP_LOGW(REST_TAG, "Free heap size before: %ld", esp_get_free_heap_size());

    return ESP_OK;
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