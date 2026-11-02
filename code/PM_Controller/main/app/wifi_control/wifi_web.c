#include "wifi_web.h"
#include "esp_http_server.h"

static httpd_handle_t server;
static esp_err_t home(httpd_req_t *request)
{
    static const char page[] =
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>PolaMiya</title><style>body{font:16px system-ui;max-width:42rem;"
        "margin:4rem auto;padding:0 1.5rem;line-height:1.6;color:#12352b}"
        "h1{font-size:2rem}small{color:#47645a}</style></head>"
        "<body><h1>PolaMiya</h1><p>设备已连接局域网。</p>"
        "<small>设备控制功能将在后续版本提供。</small></body></html>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

esp_err_t wifi_web_start(void)
{
    if (server) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t result = httpd_start(&server, &config);
    if (result != ESP_OK) { server = NULL; return result; }
    const httpd_uri_t route = {.uri = "/", .method = HTTP_GET, .handler = home};
    result = httpd_register_uri_handler(server, &route);
    if (result != ESP_OK) { httpd_stop(server); server = NULL; }
    return result;
}

void wifi_web_stop(void)
{
    if (server && httpd_stop(server) == ESP_OK) server = NULL;
}
