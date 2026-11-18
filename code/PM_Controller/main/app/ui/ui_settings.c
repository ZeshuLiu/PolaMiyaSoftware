#include "ui_settings.h"
#include "ui_menu.h"
#include "ui_menu_view.h"
#include "settings.h"
#include "lcd_st7789.h"
#include "focus_position.h"
#include "wifi_control.h"
#include "wifi_pair.h"
#include "ble_remote.h"
#include "wifi_web.h"
#include "ui_home_assets.h"
#include "ui_key_hints.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static esp_err_t read_backlight(int32_t *value)
{
    settings_values_t values;
    const esp_err_t result = settings_get(&values);
    if (result == ESP_OK) *value = (int32_t)values.backlight_percent;
    return result;
}

static esp_err_t write_backlight(int32_t value)
{
    const esp_err_t result = settings_set(SETTINGS_BACKLIGHT_PERCENT, (uint64_t)value);
    if (result == ESP_OK) lcd_st7789_set_brightness((unsigned)value);
    return result;
}

static esp_err_t read_film(int32_t *value)
{
    settings_values_t values;
    const esp_err_t result = settings_get(&values);
    if (result == ESP_OK) *value = (int32_t)values.film_remaining;
    return result;
}

static esp_err_t write_film(int32_t value)
{
    return settings_set(SETTINGS_FILM_REMAINING, (uint64_t)value);
}

static const char *const film_options[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "A"};

static esp_err_t read_wifi(int32_t *value)
{
    *value = wifi_control_is_enabled() ? 1 : 0;
    return ESP_OK;
}

static esp_err_t write_wifi(int32_t value)
{
    return wifi_control_request_enabled(value != 0);
}

static const char *const wifi_options[] = {"OFF", "ON"};

static esp_err_t toggle_pair(void)
{
    if (wifi_pair_active()) { wifi_pair_cancel(); return ESP_OK; }
    return wifi_pair_start();
}

static const char *pair_status(void)
{
    return ble_remote_active() ? "STOP BLE FIRST" : wifi_pair_status();
}

static bool pair_enabled(void) { return !ble_remote_active(); }
static bool remote_enabled(void) { return !wifi_pair_active(); }
static bool remote_preference;
static bool remote_save_failed;

static const char *remote_detail(void)
{
    return remote_preference ? "ON" : "OFF";
}

static const char *remote_status(void)
{
    if (wifi_pair_active()) return "STOP PAIR FIRST";
    return remote_preference || ble_remote_active() ? ble_remote_status() : "OFF";
}

static esp_err_t toggle_remote(void)
{
    remote_save_failed = false;
    const bool enable = !remote_preference;
    if (enable) {
        const esp_err_t result = ble_remote_start();
        if (result != ESP_OK) return result;
    } else {
        ble_remote_stop();
    }
    const esp_err_t result = settings_save_ble_remote_enabled(enable);
    remote_save_failed = result != ESP_OK;
    return result;
}

static bool home_enabled(void)
{
    settings_status_t status;
    return settings_get_status(&status) == ESP_OK && status.writable &&
           focus_position_can_home((uint32_t)(esp_timer_get_time() / 1000));
}

static esp_err_t start_home(void)
{
    return focus_position_home((uint32_t)(esp_timer_get_time() / 1000));
}

static bool web_page;
static char web_ip[16], web_port[16];
static const char *web_state;
static lv_obj_t *web_overlay, *web_ip_label, *web_port_label, *web_status_label;

static esp_err_t open_web(void)
{
    web_page = true;
    return wifi_web_request(true);
}

static const ui_menu_item_t settings_items[] = {
    {.title = "BACKLIGHT", .type = UI_MENU_NUMBER,
     .number = {.minimum = 0, .maximum = 100, .step = 5, .unit = "%",
                .read = read_backlight, .write = write_backlight}},
    {.title = "FILM", .type = UI_MENU_CHOICE,
     .choice = {.labels = film_options, .count = 10, .read = read_film, .write = write_film}},
    {.title = "NETWORK", .type = UI_MENU_PAGE},
    {.title = "SEEK ZERO", .type = UI_MENU_ACTION,
     .action = {.run = start_home, .enabled = home_enabled,
                .busy = focus_position_is_homing, .status = focus_position_message}},
    {.title = "BACK", .type = UI_MENU_BACK},
};

/* The Wi-Fi and BLE controls apply their preferences immediately. */
static const ui_menu_item_t network_items[] = {
    {.title = "WIFI", .type = UI_MENU_SUBMENU,
     .choice = {.labels = wifi_options, .count = 2, .read = read_wifi, .write = write_wifi}},
    {.title = "BLE REMOTE", .type = UI_MENU_ACTION,
     .action = {.run = toggle_remote, .enabled = remote_enabled,
                .status = remote_status, .detail = remote_detail}},
    {.title = "PAIR WIFI", .type = UI_MENU_ACTION,
     .action = {.run = toggle_pair, .enabled = pair_enabled, .status = pair_status}},
    {.title = "WEB CONFIG", .type = UI_MENU_ACTION,
     .action = {.run = open_web}},
    {.title = "BACK", .type = UI_MENU_BACK},
};
#define SETTINGS_COUNT (sizeof(settings_items) / sizeof(settings_items[0]))
#define NETWORK_COUNT (sizeof(network_items) / sizeof(network_items[0]))

static ui_menu_t settings_menu, network_menu;
static ui_menu_value_t settings_values[SETTINGS_COUNT], network_values[NETWORK_COUNT];
static bool network_page;
static settings_status_t storage;
static const char *notice;
static bool notice_error;

static ui_menu_t *active_menu(void)
{
    return network_page ? &network_menu : &settings_menu;
}

void ui_settings_refresh(void)
{
    if (web_page) {
        snprintf(web_port, sizeof(web_port), "PORT %u", (unsigned)wifi_web_port());
        if (!wifi_control_ip(web_ip, sizeof(web_ip))) {
            strcpy(web_ip, "NO IP");
            web_state = "NO NETWORK";
        } else if (wifi_web_running()) web_state = "SERVER ON";
        else if (wifi_web_error() != ESP_OK) web_state = "SERVER ERROR";
        else web_state = "STARTING";
    }
    settings_values_t preferences;
    remote_preference = settings_get(&preferences) == ESP_OK && preferences.ble_remote_enabled;
    ui_menu_refresh(active_menu());
    if (settings_get_status(&storage) != ESP_OK)
        storage = (settings_status_t){.dirty = true, .writable = false};
}

void ui_settings_init(void)
{
    ESP_ERROR_CHECK(ui_menu_init(&settings_menu, settings_items, settings_values, SETTINGS_COUNT));
    ESP_ERROR_CHECK(ui_menu_init(&network_menu, network_items, network_values, NETWORK_COUNT));
    lcd_st7789_set_brightness(settings_values[0].valid ? (unsigned)settings_values[0].value : 100);
    ui_settings_refresh();
}

void ui_settings_open(void)
{
    network_page = false;
    ui_menu_cancel(&settings_menu);
    web_page = false;
    notice = NULL;
    notice_error = false;
    ui_settings_refresh();
}

static void report_result(const ui_menu_t *menu, ui_menu_result_t result)
{
    const ui_menu_item_t *item = &menu->items[menu->selected];
    if (result == UI_MENU_CHANGED) {
        notice = item->type == UI_MENU_SUBMENU ? NULL :
                 item->type == UI_MENU_CHOICE && item->choice.write == write_film ? "AUTO SAVE" : "UNSAVED";
        notice_error = false;
    }
    else if (result == UI_MENU_STARTED) { notice = NULL; notice_error = false; }
    else if (result == UI_MENU_FAILED) {
        if (item->type == UI_MENU_ACTION)
            notice = item->action.run == toggle_remote && remote_save_failed ? "SAVE ERROR" : "NOT READY";
        else
            notice = item->type == UI_MENU_SUBMENU ? "WIFI ERROR" : "RAM ERROR";
        notice_error = true;
        ESP_LOGW("ui_settings", "%s: %s", item->title, esp_err_to_name(menu->error));
    }
}

bool ui_settings_handle_keys(hc165_key_events_t keys)
{
    if (!(keys.short_press | keys.long_press)) return false;
    if (web_page) {
        if (keys.short_press & HC165_KEY2) {
            web_page = false;
            const esp_err_t result = wifi_web_request(false);
            if (result != ESP_OK) ESP_LOGW("ui_settings", "Web stop request: %s", esp_err_to_name(result));
        }
        return false;
    }
    ui_settings_refresh();
    ui_menu_t *menu = active_menu();
    const ui_menu_item_t *item = &menu->items[menu->selected];
    if (keys.long_press & HC165_KEY2) {
        if (menu->phase == UI_MENU_LIST || (menu->phase == UI_MENU_EDIT && item->type == UI_MENU_CHOICE)) {
            ui_menu_cancel(menu); /* Cancel candidate; never save it by holding. */
            notice = NULL;
            notice_error = false;
            return false;
        }
        if (menu->phase == UI_MENU_EDIT) {
            const ui_menu_result_t result = ui_menu_confirm(menu);
            report_result(menu, result);
            if (result == UI_MENU_FAILED) return false;
        }
        /* This handler runs before display locking. All EEPROM I/O stays here. */
        const esp_err_t result = settings_commit();
        notice = result == ESP_OK ? "SAVED" : "SAVE FAILED";
        notice_error = result != ESP_OK;
        if (result != ESP_OK) ESP_LOGW("ui_settings", "Save: %s", esp_err_to_name(result));
        return false;
    }
    if (keys.short_press & HC165_KEY2) {
        if (item->type == UI_MENU_SUBMENU && item->choice.write == write_wifi) {
            ESP_LOGI("wifi", "UI confirm: phase=%u cursor=%u current=%ld",
                     (unsigned)menu->phase, menu->cursor,
                     (long)menu->values[menu->selected].value);
        }
        notice = NULL;
        notice_error = false;
        const ui_menu_result_t result = ui_menu_confirm(menu);
        report_result(menu, result);
        if (result == UI_MENU_ENTER) {
            ui_menu_cancel(&network_menu);
            network_menu.selected = 0;
            network_page = true;
        } else if (result == UI_MENU_LEAVE) {
            wifi_pair_cancel();
            if (!network_page) return true;
            network_page = false;
        }
        return false;
    }
    const bool up = (keys.short_press & HC165_KEY3) != 0;
    const bool down = (keys.short_press & HC165_KEY1) != 0;
    if (up != down) {
        notice = NULL;
        notice_error = false;
        const ui_menu_result_t result = ui_menu_move(menu, up ? -1 : 1);
        report_result(menu, result);
    }
    return false;
}

lv_obj_t *ui_settings_create(lv_obj_t *screen)
{
    lv_obj_t *root = ui_menu_view_create(screen, &settings_menu, &network_menu);
    web_overlay = lv_obj_create(root);
    lv_obj_remove_style_all(web_overlay);
    lv_obj_remove_flag(web_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(web_overlay, 0, 0);
    lv_obj_set_size(web_overlay, 136, 320);
    lv_obj_set_style_bg_color(web_overlay, lv_color_hex(0x080f10), 0);
    lv_obj_set_style_bg_opa(web_overlay, LV_OPA_COVER, 0);
    const char *texts[] = {"WEB CONFIG", "IP ADDRESS", "KEY2: EXIT"};
    const int ys[] = {8, 54, 252};
    for (unsigned i = 0; i < 3; ++i) {
        lv_obj_t *label = lv_label_create(web_overlay);
        lv_obj_set_pos(label, 12, ys[i]);
        lv_obj_set_width(label, 116);
        lv_obj_set_style_text_font(label, &ui_font_body_bold, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xe0eee7), 0);
        lv_label_set_text(label, texts[i]);
    }
    web_ip_label = lv_label_create(web_overlay);
    lv_obj_set_pos(web_ip_label, 12, 80);
    lv_obj_set_width(web_ip_label, 116);
    lv_obj_set_style_text_font(web_ip_label, &ui_font_body_bold, 0);
    lv_obj_set_style_text_color(web_ip_label, lv_color_hex(0x79f5a7), 0);
    lv_label_set_long_mode(web_ip_label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_text(web_ip_label, "NO IP");
    web_port_label = lv_label_create(web_overlay);
    lv_obj_set_pos(web_port_label, 12, 128);
    lv_obj_set_width(web_port_label, 116);
    lv_obj_set_style_text_font(web_port_label, &ui_font_body_bold, 0);
    lv_obj_set_style_text_color(web_port_label, lv_color_hex(0xe0eee7), 0);
    lv_label_set_text(web_port_label, "PORT --");
    web_status_label = lv_label_create(web_overlay);
    lv_obj_set_pos(web_status_label, 12, 180);
    lv_obj_set_width(web_status_label, 116);
    lv_obj_set_style_text_font(web_status_label, &ui_font_body_bold, 0);
    lv_obj_set_style_text_color(web_status_label, lv_color_hex(0x79f5a7), 0);
    lv_label_set_text(web_status_label, "STARTING");
    lv_obj_add_flag(web_overlay, LV_OBJ_FLAG_HIDDEN);
    return root;
}

void ui_settings_render(void)
{
    lv_obj_set_flag(web_overlay, LV_OBJ_FLAG_HIDDEN, !web_page);
    if (web_page) {
        if (strcmp(lv_label_get_text(web_ip_label), web_ip) != 0) lv_label_set_text(web_ip_label, web_ip);
        if (strcmp(lv_label_get_text(web_port_label), web_port) != 0) lv_label_set_text(web_port_label, web_port);
        if (web_state && strcmp(lv_label_get_text(web_status_label), web_state) != 0)
            lv_label_set_text(web_status_label, web_state);
        ui_key_hints_menu(UI_MENU_HINT_CHOICE);
        return;
    }
    const ui_menu_t *menu = active_menu();
    const ui_menu_item_t *item = &menu->items[menu->selected];
    const ui_menu_value_t *value = &menu->values[menu->selected];
    static char wifi_ip[16];
    ui_menu_footer_t footer = {.status = "SAVED", .hint = "HOLD TO SAVE", .color = 0x79f5a7};
    if (!storage.writable) { footer.status = "STORE ERROR"; footer.color = 0xffbe58; }
    else if (storage.dirty) { footer.status = "UNSAVED"; footer.color = 0xffbe58; }
    if (item->type == UI_MENU_ACTION) {
        footer.status = value->status ? value->status : "READY";
        if (strcmp(footer.status, "HOME REQUIRED") == 0) footer.status = "SEEK ZERO";
        footer.hint = item->action.run == toggle_pair && wifi_pair_active() ? "PRESS TO STOP" :
                      value->busy ? "PLEASE WAIT" : value->enabled ? "PRESS TO RUN" : "PLEASE WAIT";
        if (item->action.run == toggle_remote && value->enabled)
            footer.hint = remote_preference ? "PRESS FOR OFF" : "PRESS FOR ON";
        footer.color = value->busy ? 0x69d6ff : 0x91afa0;
    } else if (item->type == UI_MENU_BACK) {
        footer.status = network_page ? "RETURN SETUP" : "RETURN HOME";
        footer.hint = "PRESS OK";
        footer.color = 0x91afa0;
    } else if (menu->phase == UI_MENU_EDIT && item->type == UI_MENU_CHOICE) {
        footer.status = "";
        footer.hint = "PRESS TO SET";
        footer.color = 0x91afa0;
    } else if (item->type == UI_MENU_PAGE) {
        if (!storage.dirty) footer.status = "";
        footer.hint = "PRESS TO OPEN";
    } else if (item->type == UI_MENU_SUBMENU) {
        if (!storage.dirty) footer.status = "";
        footer.hint = "PRESS TO OPEN";
        if (wifi_control_busy()) {
            footer.status = "WIFI WAIT";
            footer.color = 0x69d6ff;
        } else if (wifi_control_last_error() != ESP_OK) {
            footer.status = "WIFI ERROR";
            footer.color = 0xffbe58;
        } else if (wifi_control_ip(wifi_ip, sizeof(wifi_ip))) {
            footer.status = wifi_ip;
            footer.color = 0x79f5a7;
        } else if (wifi_control_is_enabled() && !wifi_control_has_network()) {
            footer.status = "NO NETWORK";
            footer.color = 0xffbe58;
        }
    }
    if (!value->valid) { footer.status = "READ ERROR"; footer.color = 0xffbe58; }
    const bool showing_remote_pin = item->type == UI_MENU_ACTION &&
        item->action.run == toggle_remote && strncmp(footer.status, "PIN ", 4) == 0;
    if (notice && !value->busy && !showing_remote_pin) {
        footer.status = notice;
        footer.color = notice_error || strcmp(notice, "UNSAVED") == 0 ? 0xffbe58 : 0x79f5a7;
    }
    if (!storage.writable && !(item->type == UI_MENU_ACTION &&
                               ((item->action.run == toggle_pair && wifi_pair_active()) ||
                                item->action.run == toggle_remote))) {
        footer.status = "STORE ERROR";
        footer.hint = "RAM ONLY";
        footer.color = 0xffbe58;
    }
    ui_menu_view_update(menu, &footer, network_page);
}
