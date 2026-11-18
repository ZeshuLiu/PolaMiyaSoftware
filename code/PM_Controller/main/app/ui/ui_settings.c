#include "ui_settings.h"
#include "ui_menu.h"
#include "ui_menu_view.h"
#include "settings.h"
#include "lcd_st7789.h"
#include "focus_position.h"
#include "wifi_control.h"
#include "wifi_pair.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

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
    return wifi_pair_status();
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

/* Wi-Fi state is applied immediately; it is not an EEPROM setting. */
static const ui_menu_item_t items[] = {
    {.title = "BACKLIGHT", .type = UI_MENU_NUMBER,
     .number = {.minimum = 0, .maximum = 100, .step = 5, .unit = "%",
                .read = read_backlight, .write = write_backlight}},
    {.title = "FILM", .type = UI_MENU_CHOICE,
     .choice = {.labels = film_options, .count = 10, .read = read_film, .write = write_film}},
    {.title = "WIFI", .type = UI_MENU_SUBMENU,
     .choice = {.labels = wifi_options, .count = 2, .read = read_wifi, .write = write_wifi}},
    {.title = "PAIR WIFI", .type = UI_MENU_ACTION,
     .action = {.run = toggle_pair, .status = pair_status}},
    {.title = "SEEK ZERO", .type = UI_MENU_ACTION,
     .action = {.run = start_home, .enabled = home_enabled,
                .busy = focus_position_is_homing, .status = focus_position_message}},
    {.title = "BACK", .type = UI_MENU_BACK},
};
#define ITEM_COUNT (sizeof(items) / sizeof(items[0]))

static ui_menu_t menu;
static ui_menu_value_t values[ITEM_COUNT];
static settings_status_t storage;
static const char *notice;
static bool notice_error;

void ui_settings_refresh(void)
{
    ui_menu_refresh(&menu);
    if (settings_get_status(&storage) != ESP_OK)
        storage = (settings_status_t){.dirty = true, .writable = false};
}

void ui_settings_init(void)
{
    ESP_ERROR_CHECK(ui_menu_init(&menu, items, values, ITEM_COUNT));
    lcd_st7789_set_brightness(values[0].valid ? (unsigned)values[0].value : 100);
    ui_settings_refresh();
}

void ui_settings_open(void)
{
    ui_menu_cancel(&menu);
    notice = NULL;
    notice_error = false;
    ui_settings_refresh();
}

static void report_result(ui_menu_result_t result)
{
    if (result == UI_MENU_CHANGED) {
        const ui_menu_item_t *item = &menu.items[menu.selected];
        notice = item->type == UI_MENU_SUBMENU ? NULL :
                 item->type == UI_MENU_CHOICE && item->choice.write == write_film ? "AUTO SAVE" : "UNSAVED";
        notice_error = false;
    }
    else if (result == UI_MENU_STARTED) { notice = NULL; notice_error = false; }
    else if (result == UI_MENU_FAILED) {
        notice = menu.items[menu.selected].type == UI_MENU_ACTION ? "NOT READY" :
                 menu.items[menu.selected].type == UI_MENU_SUBMENU ? "WIFI ERROR" : "RAM ERROR";
        notice_error = true;
        ESP_LOGW("ui_settings", "%s: %s", menu.items[menu.selected].title, esp_err_to_name(menu.error));
    }
}

bool ui_settings_handle_keys(hc165_key_events_t keys)
{
    if (!(keys.short_press | keys.long_press)) return false;
    ui_settings_refresh();
    if (keys.long_press & HC165_KEY2) {
        if (menu.phase == UI_MENU_LIST || (menu.phase == UI_MENU_EDIT &&
            menu.items[menu.selected].type == UI_MENU_CHOICE)) {
            ui_menu_cancel(&menu); /* Cancel candidate; never save it by holding. */
            notice = NULL;
            notice_error = false;
            return false;
        }
        if (menu.phase == UI_MENU_EDIT) {
            const ui_menu_result_t result = ui_menu_confirm(&menu);
            report_result(result);
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
        if (menu.items[menu.selected].type == UI_MENU_SUBMENU &&
            menu.items[menu.selected].choice.write == write_wifi) {
            ESP_LOGI("wifi", "UI confirm: phase=%u cursor=%u current=%ld",
                     (unsigned)menu.phase, menu.cursor,
                     (long)menu.values[menu.selected].value);
        }
        notice = NULL;
        notice_error = false;
        const ui_menu_result_t result = ui_menu_confirm(&menu);
        report_result(result);
        if (result == UI_MENU_LEAVE) { wifi_pair_cancel(); return true; }
        return false;
    }
    const bool up = (keys.short_press & HC165_KEY3) != 0;
    const bool down = (keys.short_press & HC165_KEY1) != 0;
    if (up != down) {
        notice = NULL;
        notice_error = false;
        report_result(ui_menu_move(&menu, up ? -1 : 1));
    }
    return false;
}

lv_obj_t *ui_settings_create(lv_obj_t *screen)
{
    return ui_menu_view_create(screen, &menu);
}

void ui_settings_render(void)
{
    const ui_menu_item_t *item = &menu.items[menu.selected];
    const ui_menu_value_t *value = &menu.values[menu.selected];
    static char wifi_ip[16];
    ui_menu_footer_t footer = {.status = "SAVED", .hint = "HOLD TO SAVE", .color = 0x79f5a7};
    if (!storage.writable) { footer.status = "STORE ERROR"; footer.color = 0xffbe58; }
    else if (storage.dirty) { footer.status = "UNSAVED"; footer.color = 0xffbe58; }
    if (item->type == UI_MENU_ACTION) {
        footer.status = value->status ? value->status : "READY";
        if (strcmp(footer.status, "HOME REQUIRED") == 0) footer.status = "SEEK ZERO";
        footer.hint = item->action.run == toggle_pair && wifi_pair_active() ? "PRESS TO STOP" :
                      value->busy ? "PLEASE WAIT" : value->enabled ? "PRESS TO RUN" : "PLEASE WAIT";
        footer.color = value->busy ? 0x69d6ff : 0x91afa0;
    } else if (item->type == UI_MENU_BACK) {
        footer.status = "RETURN HOME";
        footer.hint = "PRESS OK";
        footer.color = 0x91afa0;
    } else if (menu.phase == UI_MENU_EDIT && item->type == UI_MENU_CHOICE) {
        footer.status = "";
        footer.hint = "PRESS TO SET";
        footer.color = 0x91afa0;
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
    if (notice && !value->busy) {
        footer.status = notice;
        footer.color = notice_error || strcmp(notice, "UNSAVED") == 0 ? 0xffbe58 : 0x79f5a7;
    }
    if (!storage.writable && !(item->type == UI_MENU_ACTION &&
                               item->action.run == toggle_pair && wifi_pair_active())) {
        footer.status = "STORE ERROR";
        footer.hint = "RAM ONLY";
        footer.color = 0xffbe58;
    }
    ui_menu_view_update(&menu, &footer);
}
