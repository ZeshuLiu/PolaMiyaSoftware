#include "boot_screen.h"

#include "lcd_st7789.h"
#include "ui_home_assets.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_mac.h"

static const char *TAG = "boot_screen";
static const char *const stage_names[BOOT_STAGE_COUNT] = {
    "DISPLAY SYS", "TRIGGER SYS", "KEY INPUT", "RANGE SENSOR", "PARAM STORE", "ADC INPUT",
};

static struct {
    lv_display_t *display;
    lv_obj_t *heading;
    lv_obj_t *names[BOOT_STAGE_COUNT];
    lv_obj_t *dots[BOOT_STAGE_COUNT];
    lv_obj_t *results[BOOT_STAGE_COUNT];
    boot_stage_t current;
    bool warning;
} boot;

static lv_obj_t *text(lv_obj_t *parent, int x, int y, int width,
                      const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, font->line_height);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *rectangle(lv_obj_t *parent, int x, int y, int width,
                           int height, int radius, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

static lv_obj_t *image(lv_obj_t *parent, int x, int y,
                       const lv_image_dsc_t *source)
{
    lv_obj_t *obj = lv_image_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_src(obj, source);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

/* Caller holds the port lock. Force the changed region out before the startup
 * task enters another driver call; the official port still owns SPI/DMA. */
static void paint_result(esp_err_t result)
{
    if (result != ESP_OK) boot.warning = true;
    lv_label_set_text(boot.names[boot.current], stage_names[boot.current]);
    lv_label_set_text(boot.dots[boot.current], "....");
    lv_label_set_text(boot.results[boot.current], result == ESP_OK ? "[OK]" : "[FAIL]");
    lv_obj_set_style_text_color(boot.results[boot.current],
                                lv_color_hex(result == ESP_OK ? 0xb1ffd3 : 0xffa24d), 0);
    lv_obj_set_style_text_color(boot.names[boot.current],
                                lv_color_hex(result == ESP_OK ? 0xa9bbb4 : 0xffa24d), 0);
    lv_refr_now(boot.display);
}

void boot_screen_start(lv_display_t *display)
{
    uint8_t mac[6];
    const esp_err_t mac_result = esp_efuse_mac_get_default(mac);
    if (mac_result != ESP_OK) {
        ESP_LOGW(TAG, "Factory MAC read failed: %s", esp_err_to_name(mac_result));
    }
    boot.display = display;
    boot.current = BOOT_STAGE_DISPLAY;
    boot.warning = mac_result != ESP_OK;

    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, &ui_font_boot_label, 0);

    image(screen, 12, 8, &ui_boot_header);
    rectangle(screen, 28, 48, 116, 64, 14, 0xffffff);
    lv_obj_t *dog = image(screen, 36, 56, &ui_boot_dog);
    /* LVGL blends the A8 mask directly: black ink over the white native card. */
    lv_obj_set_style_image_recolor(dog, lv_color_black(), 0);
    lv_obj_set_style_image_recolor_opa(dog, LV_OPA_COVER, 0);
    rectangle(screen, 14, 132, 3, 3, 0, 0x789084);
    rectangle(screen, 14, 147, 144, 1, 0, 0x17241f);
    rectangle(screen, 14, 287, 144, 1, 0, 0x17241f);

    boot.heading = text(screen, 23, 128, 135, &ui_font_boot_label, 0x789084);
    lv_label_set_text(boot.heading, "SYSTEM STARTUP");
    for (unsigned i = 0; i < BOOT_STAGE_COUNT; ++i) {
        const int y = 156 + (int)i * 20;
        boot.names[i] = text(screen, 14, y, 80, &ui_font_boot_log, 0xa9bbb4);
        boot.dots[i] = text(screen, 94, y, 24, &ui_font_boot_log, 0x40574b);
        boot.results[i] = text(screen, 122, y, 36, &ui_font_boot_log, 0xb1ffd3);
        lv_obj_set_style_text_letter_space(boot.names[i], 1, 0);
        lv_obj_set_style_text_letter_space(boot.dots[i], 1, 0);
        lv_obj_set_style_text_letter_space(boot.results[i], 1, 0);
        lv_obj_set_style_text_align(boot.results[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(boot.names[i], "");
        lv_label_set_text(boot.dots[i], "");
        lv_label_set_text(boot.results[i], "");
    }
    lv_obj_t *mac_text = text(screen, 22, 298, 128, &ui_font_boot_label, 0x789084);
    lv_obj_set_style_text_align(mac_text, LV_TEXT_ALIGN_CENTER, 0);
    if (mac_result == ESP_OK) {
        lv_label_set_text_fmt(mac_text, "MAC : %02X:%02X:%02X:%02X:%02X:%02X",
                              (unsigned)mac[0], (unsigned)mac[1], (unsigned)mac[2],
                              (unsigned)mac[3], (unsigned)mac[4], (unsigned)mac[5]);
    } else {
        lv_label_set_text(mac_text, "MAC : --");
    }
    paint_result(ESP_OK);
    lvgl_port_unlock();
    /* Before settings have loaded, the driver uses its startup brightness.
     * ui_start() applies the saved value without restarting the fade. */
    lcd_st7789_backlight(true);
}

void boot_screen_begin(boot_stage_t stage)
{
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    boot.current = stage;
    lv_label_set_text(boot.names[stage], stage_names[stage]);
    lv_obj_set_style_text_color(boot.names[stage], lv_color_hex(0xf1f5f4), 0);
    lv_label_set_text(boot.dots[stage], "....");
    lv_label_set_text(boot.results[stage], "[...]");
    lv_obj_set_style_text_color(boot.results[stage], lv_color_hex(0xffa24d), 0);
    lv_refr_now(boot.display);
    lvgl_port_unlock();
}

void boot_screen_complete(esp_err_t result)
{
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    paint_result(result);
    lvgl_port_unlock();
}

void boot_screen_finish(void)
{
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    lv_label_set_text(boot.heading, boot.warning ? "READY / WARN" : "SYSTEM READY");
    lv_obj_set_style_text_color(boot.heading,
                                lv_color_hex(boot.warning ? 0xffa24d : 0x9caeab), 0);
    lv_refr_now(boot.display);
    lvgl_port_unlock();
    /* Objects remain on the active screen until ui_view_create() cleans it. */
    boot.display = NULL;
}
