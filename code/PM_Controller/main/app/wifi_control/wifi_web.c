#include "wifi_web.h"
#include "wifi_control.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "focus_calibration.h"
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>

static httpd_handle_t server;
static atomic_bool desired_running;
static atomic_bool network_available;
static atomic_int last_error;
static atomic_uint desired_port;
static atomic_uint active_port;

static bool web_allowed(void) { return atomic_load(&desired_running) && atomic_load(&network_available); }

static esp_err_t home(httpd_req_t *request)
{
    if (!web_allowed()) return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Closed");
    static const char page[] =
        "<!doctype html><html lang='zh-CN'><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>PolaMiya 对焦标定</title><style>body{font:16px system-ui;max-width:36rem;"
        "margin:2rem auto;padding:0 1rem;line-height:1.6;color:#12352b}"
        "label{display:block;margin:1rem 0}input{display:block;width:100%;box-sizing:border-box;"
        "padding:.5rem}button{padding:.7rem;margin-right:.5rem}</style>"
        "<h1>对焦标定</h1><p>RAW = OFFSET + A / (d - B)，距离 d 单位：米。</p>"
        "<p>A 必须大于 0；B 必须小于最小距离（米）。最小距离 1–20000 mm，容差 0–10000 RAW。</p>"
        "<form id='f'><label>OFFSET（RAW）<input id='offset' type='number' step='any' required></label>"
        "<label>A（RAW·m）<input id='a' type='number' step='any' required></label>"
        "<label>B（m）<input id='b' type='number' step='any' required></label>"
        "<label>最小距离（mm）<input id='min' type='number' step='1' required></label>"
        "<label>无穷远显示容差（RAW）<input id='tol' type='number' step='1' required></label>"
        "<button>保存</button><button type='button' id='reset'>恢复默认值</button></form><p id='msg'></p>"
        "<script>const ids=['offset','a','b','min','tol'],msg=document.querySelector('#msg');"
        "async function load(){let r=await fetch('/calibration');if(!r.ok)throw Error(await r.text());"
        "let v=await r.json();ids.forEach(k=>document.getElementById(k).value=v[k])}"
        "document.querySelector('#f').onsubmit=async e=>{e.preventDefault();try{"
        "let r=await fetch('/calibration',{method:'POST',headers:{'Content-Type':'text/plain'},"
        "body:ids.map(k=>document.getElementById(k).value).join('\\n')});"
        "msg.textContent=await r.text();if(r.ok)await load()}catch(e){msg.textContent='连接失败'}};"
        "document.querySelector('#reset').onclick=async()=>{try{let r=await fetch('/defaults',{method:'POST'});"
        "msg.textContent=await r.text();if(r.ok)await load()}catch(e){msg.textContent='连接失败'}};"
        "load().catch(()=>msg.textContent='读取失败');</script></html>";
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t read_calibration(httpd_req_t *request)
{
    if (!web_allowed()) return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Closed");
    focus_calibration_t c;
    if (focus_calibration_get(&c) != ESP_OK)
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Read failed");
    char json[256];
    snprintf(json, sizeof(json), "{\"offset\":%.17g,\"a\":%.17g,\"b\":%.17g,\"min\":%lu,\"tol\":%lu}",
             c.offset_raw, c.a_raw_m, c.b_m, (unsigned long)c.min_distance_mm,
             (unsigned long)c.infinity_tolerance_raw);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, json);
}

static esp_err_t save_result(httpd_req_t *request, const focus_calibration_t *c)
{
    if (!web_allowed()) return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Closed");
    if (!focus_calibration_valid(c)) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid calibration");
    const esp_err_t result = focus_calibration_save(c);
    if (result != ESP_OK) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "EEPROM save failed");
    return httpd_resp_sendstr(request, "Saved");
}

static esp_err_t write_calibration(httpd_req_t *request)
{
    if (!web_allowed()) return httpd_resp_send_err(request, HTTPD_403_FORBIDDEN, "Closed");
    if (request->content_len == 0 || request->content_len >= 256)
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid length");
    char body[256];
    size_t used = 0;
    while (used < request->content_len) {
        int n = httpd_req_recv(request, body + used, request->content_len - used);
        if (n <= 0) return ESP_FAIL;
        used += (size_t)n;
    }
    body[used] = 0;
    focus_calibration_t c;
    unsigned long long min, tol;
    int end = 0;
    if (sscanf(body, "%lf %lf %lf %llu %llu %n", &c.offset_raw, &c.a_raw_m, &c.b_m,
               &min, &tol, &end) != 5 || body[end] != 0 || min > UINT32_MAX || tol > UINT32_MAX)
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid numbers");
    c.min_distance_mm = (uint32_t)min;
    c.infinity_tolerance_raw = (uint32_t)tol;
    return save_result(request, &c);
}

static esp_err_t restore_defaults(httpd_req_t *request)
{
    const focus_calibration_t c = focus_calibration_defaults();
    return save_result(request, &c);
}

static esp_err_t wifi_web_start(uint16_t port)
{
    if (server) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.stack_size = 8192;
    esp_err_t result = httpd_start(&server, &config);
    if (result != ESP_OK) { server = NULL; return result; }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = home},
        {.uri = "/calibration", .method = HTTP_GET, .handler = read_calibration},
        {.uri = "/calibration", .method = HTTP_POST, .handler = write_calibration},
        {.uri = "/defaults", .method = HTTP_POST, .handler = restore_defaults},
    };
    for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]) && result == ESP_OK; ++i)
        result = httpd_register_uri_handler(server, &routes[i]);
    if (result != ESP_OK) { httpd_stop(server); server = NULL; }
    else atomic_store(&active_port, port);
    return result;
}

static esp_err_t wifi_web_stop(void)
{
    if (!server) return ESP_OK;
    const esp_err_t result = httpd_stop(server);
    if (result == ESP_OK) { server = NULL; atomic_store(&active_port, 0); }
    return result;
}

/* Only the network worker owns the HTTP server handle. The requested state
 * is published separately so exiting the page rejects new requests at once. */
void wifi_web_process(void)
{
    const bool requested = web_allowed();
    const uint16_t port = (uint16_t)atomic_load(&desired_port);
    esp_err_t result = ESP_OK;
    if (server && (!requested || atomic_load(&active_port) != port)) result = wifi_web_stop();
    if (result == ESP_OK && requested && !server) result = wifi_web_start(port);
    if (result != ESP_OK) ESP_LOGW("wifi_web", "%s failed: %s", requested ? "Start" : "Stop", esp_err_to_name(result));
    atomic_store(&last_error, result);
}

esp_err_t wifi_web_request(bool enabled)
{
    const esp_err_t result = wifi_control_prepare();
    if (result != ESP_OK) return result;
    if (enabled) {
        atomic_store(&desired_port, 49152U + (esp_random() & 0x3fffU));
        atomic_store(&last_error, ESP_OK);
    }
    atomic_store(&desired_running, enabled);
    return wifi_control_notify_web();
}

esp_err_t wifi_web_set_network(bool available)
{
    atomic_store(&network_available, available);
    return wifi_control_notify_web();
}

bool wifi_web_running(void)
{
    const unsigned active = atomic_load(&active_port);
    return active != 0 && web_allowed() && active == atomic_load(&desired_port);
}
uint16_t wifi_web_port(void) { return (uint16_t)atomic_load(&desired_port); }
esp_err_t wifi_web_error(void) { return (esp_err_t)atomic_load(&last_error); }
