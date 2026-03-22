#include "lvgl_usr.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

/* 外部数据声明 */
extern double BAT_V;           // 来自 adc.c
extern float temperature_cd;   // 来自 adc.c (核心温度)
extern float tmp102Temp;       // 来自 tmp102.c (板载温度)
extern float distance;         // 来自 sdm18.c
extern char RBC_vals[6];       // 来自 DataStore.c
extern char MTL_vals[6];       // 来自 DataStore.c
extern char STC_vals[6];       // 来自 DataStore.c
extern char keyval_vals[4];    // 来自 zui_usr.c

/* 电机控制外部声明 */
extern void motor_start(void);
extern uint8_t motor_state_delay;

/* 显示数据缓冲区 */
char lvgl_bat_vals[6] = "-.--V";
char lvgl_key_vals[4] = "x-x";
char lvgl_distance_vals[6] = "xx.xx";
char lvgl_core_temp_vals[3] = "xx";
char lvgl_board_temp_vals[3] = "xx";
char lvgl_rbc_vals[6] = "00000";
char lvgl_mtl_vals[6] = "00000";
char lvgl_stc_vals[6] = "00000";

/* UI 对象引用 */
static lv_obj_t *bat_label = NULL;
static lv_obj_t *bat_val_label = NULL;
static lv_obj_t *key_label = NULL;
static lv_obj_t *key_val_label = NULL;
static lv_obj_t *distance_label = NULL;
static lv_obj_t *distance_val_label = NULL;
static lv_obj_t *core_temp_label = NULL;
static lv_obj_t *core_temp_val_label = NULL;
static lv_obj_t *board_temp_label = NULL;
static lv_obj_t *board_temp_val_label = NULL;
static lv_obj_t *rbc_label = NULL;
static lv_obj_t *rbc_val_label = NULL;
static lv_obj_t *mtl_label = NULL;
static lv_obj_t *mtl_val_label = NULL;
static lv_obj_t *stc_label = NULL;
static lv_obj_t *stc_val_label = NULL;
static lv_obj_t *footer_label = NULL;
static lv_obj_t *cal_scale_label = NULL;

/* 样式定义 */
static lv_style_t style_title;
static lv_style_t style_value;
static lv_style_t style_footer;
static lv_style_t style_cal_scale;

/* 布局参数 - 与 ZUI 保持一致 */
#define TITLE_X     30
#define VALUE_X     67      // 30 + 37
#define START_Y     30
#define ROW_HEIGHT  20

/* 自定义浮点标签函数 */
static void lv_label_set_float(lv_obj_t *label, float x)
{
    int a = (int)x;
    int b = (int)((x - a) * 100);

    if(b < 0) b = -b;  // 处理负数

    lv_label_set_text_fmt(label, "%d.%02d", a, b);
}

/* 自定义浮点标签函数（带单位后缀） */
static void lv_label_set_float_v(lv_obj_t *label, float x)
{
    int a = (int)x;
    int b = (int)((x - a) * 100);

    if(b < 0) b = -b;  // 处理负数

    lv_label_set_text_fmt(label, "%d.%02dV", a, b);
}

/* 初始化样式 */
static void init_styles(void)
{
    /* 标题样式 (16 号字体类似) */
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_title, lv_color_black());

    /* 数值样式 (16 号字体类似) */
    lv_style_init(&style_value);
    lv_style_set_text_font(&style_value, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_value, lv_color_black());

    /* 底部样式 (12 号字体类似) */
    lv_style_init(&style_footer);
    lv_style_set_text_font(&style_footer, &lv_font_montserrat_12);
    lv_style_set_text_color(&style_footer, lv_color_black());

    /* 焦距刻度样式 (12 号字体类似) */
    lv_style_init(&style_cal_scale);
    lv_style_set_text_font(&style_cal_scale, &lv_font_montserrat_12);
    lv_style_set_text_color(&style_cal_scale, lv_color_black());
}

/* 创建显示行 */
static lv_obj_t* create_display_row(lv_obj_t *parent, const char *title,
                                     lv_obj_t **title_label, lv_obj_t **value_label,
                                     int y_pos, lv_style_t *title_style, lv_style_t *value_style)
{
    /* 创建标题标签 */
    *title_label = lv_label_create(parent);
    lv_label_set_text(*title_label, title);
    lv_obj_set_pos(*title_label, TITLE_X, y_pos);
    lv_obj_add_style(*title_label, title_style, 0);

    /* 创建数值标签 */
    *value_label = lv_label_create(parent);
    lv_label_set_text(*value_label, "--");
    lv_obj_set_pos(*value_label, VALUE_X, y_pos);
    lv_obj_add_style(*value_label, value_style, 0);

    return *value_label;
}

void lvgl_init_ui()
{
    lv_init();
    lv_port_disp_init();                   // 注册 LVGL 的显示任务
    lv_port_indev_init();                  // 注册 LVGL 的触屏检测任务

    /* 初始化样式 */
    init_styles();

    /* 创建主容器 */
    lv_obj_t *main_cont = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(main_cont);
    lv_obj_set_size(main_cont, 280, 240);
    lv_obj_set_style_bg_color(main_cont, lv_color_make(255, 0, 255), 0);  // MAGENTA 背景

    /* 创建各显示行 - 与 ZUI 布局保持一致 */
    create_display_row(main_cont, "BAT:", &bat_label, &bat_val_label, START_Y, &style_title, &style_value);
    create_display_row(main_cont, "KEY:", &key_label, &key_val_label, START_Y + ROW_HEIGHT, &style_title, &style_value);
    create_display_row(main_cont, "DIS:", &distance_label, &distance_val_label, START_Y + 2 * ROW_HEIGHT, &style_title, &style_value);
    create_display_row(main_cont, "CTP:", &core_temp_label, &core_temp_val_label, START_Y + 3 * ROW_HEIGHT, &style_title, &style_value);
    create_display_row(main_cont, "BTP:", &board_temp_label, &board_temp_val_label, START_Y + 4 * ROW_HEIGHT, &style_title, &style_value);
    create_display_row(main_cont, "RBC:", &rbc_label, &rbc_val_label, START_Y + 5 * ROW_HEIGHT, &style_title, &style_value);
    create_display_row(main_cont, "MTL:", &mtl_label, &mtl_val_label, START_Y + 6 * ROW_HEIGHT, &style_title, &style_value);
    create_display_row(main_cont, "STC:", &stc_label, &stc_val_label, START_Y + 7 * ROW_HEIGHT, &style_title, &style_value);

    /* 创建焦距刻度 (y=200) */
    cal_scale_label = lv_label_create(main_cont);
    lv_label_set_text(cal_scale_label, "inf-7m-5m-4m-3.5m-3m-2.5m-2m-1.7m-1.5m-1.3m-1.2m-1.05m-1m");
    lv_obj_set_pos(cal_scale_label, 20, 200);
    lv_obj_set_width(cal_scale_label, 240);
    lv_label_set_long_mode(cal_scale_label, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_add_style(cal_scale_label, &style_cal_scale, 0);

    /* 创建底部版权信息 (y=220) */
    footer_label = lv_label_create(main_cont);
    lv_label_set_text(footer_label, "Powered By LVGL (Ver 0.0.1)");
    lv_obj_set_pos(footer_label, 20, 220);
    lv_obj_add_style(footer_label, &style_footer, 0);

    /* 初始化数值显示 */
    lv_label_set_text(bat_val_label, lvgl_bat_vals);
    lv_label_set_text(key_val_label, lvgl_key_vals);
    lv_label_set_text(distance_val_label, lvgl_distance_vals);
    lv_label_set_text(core_temp_val_label, lvgl_core_temp_vals);
    lv_label_set_text(board_temp_val_label, lvgl_board_temp_vals);
    lv_label_set_text(rbc_val_label, lvgl_rbc_vals);
    lv_label_set_text(mtl_val_label, lvgl_mtl_vals);
    lv_label_set_text(stc_val_label, lvgl_stc_vals);
}

/* 更新显示内容 - 需要在主循环中调用 */
void lvgl_update_display(void)
{
    /* BAT_V: 电池电压 */
    if (BAT_V > 0.1f && BAT_V < 10.0f) {
        lv_label_set_float_v(bat_val_label, (float)BAT_V);
        lv_strcpy(lvgl_bat_vals, lv_label_get_text(bat_val_label));
    } else {
        lv_label_set_text(bat_val_label, "-.--V");
        lv_strcpy(lvgl_bat_vals, "-.--V");
    }

    /* keyval_vals: 按键状态 - 在 lvgl_on_key 中更新 */
    if (key_val_label) lv_label_set_text(key_val_label, keyval_vals);

    /* distance: 距离 ( SDM18 ) */
    if (distance >= 0 && distance < 20.0f) {
        lv_label_set_float(distance_val_label, distance);
        lv_strcpy(lvgl_distance_vals, lv_label_get_text(distance_val_label));
    }
    else if (distance >= 20.0f){
        lv_label_set_text(distance_val_label, "Infinity");
        lv_strcpy(lvgl_distance_vals, "Infinity");
    }
    else {
        lv_label_set_text(distance_val_label, "xx.xx");
        lv_strcpy(lvgl_distance_vals, "xx.xx");
    }

    /* temperature_cd: 核心温度 */
    if (temperature_cd >= 0.0f && temperature_cd < 100.0f) {
        int temp_int = (int)(temperature_cd + 0.5f);
        if (temp_int >= 0 && temp_int < 100) {
            lvgl_core_temp_vals[0] = (temp_int / 10) + '0';
            lvgl_core_temp_vals[1] = (temp_int % 10) + '0';
            lvgl_core_temp_vals[2] = '\0';
        }
        if (core_temp_val_label) lv_label_set_text(core_temp_val_label, lvgl_core_temp_vals);
    }

    /* tmp102Temp: 板载温度 */
    if (tmp102Temp >= 0.0f && tmp102Temp < 100.0f) {
        int temp_int = (int)(tmp102Temp + 0.5f);
        if (temp_int >= 0 && temp_int < 100) {
            lvgl_board_temp_vals[0] = (temp_int / 10) + '0';
            lvgl_board_temp_vals[1] = (temp_int % 10) + '0';
            lvgl_board_temp_vals[2] = '\0';
        }
        if (board_temp_val_label) lv_label_set_text(board_temp_val_label, lvgl_board_temp_vals);
    }

    /* RBC/MTL/STC: 从 EEPROM 数据同步 */
    if (rbc_val_label) lv_label_set_text(rbc_val_label, RBC_vals);
    if (mtl_val_label) lv_label_set_text(mtl_val_label, MTL_vals);
    if (stc_val_label) lv_label_set_text(stc_val_label, STC_vals);
}

/* 按键响应 - 与 ZUI 保持一致 */
void lvgl_on_key(uint8_t key_index, uint8_t key_action)
{
    /* 更新按键状态显示 */
    keyval_vals[0] = key_index + '0';
    keyval_vals[2] = key_action + '0';
    if (key_val_label) {
        lv_label_set_text(key_val_label, keyval_vals);
    }

    /* 按键响应：KEY0 长按启动电机 */
    if (motor_state_delay == 0 && key_index == 0 && key_action == 2) {
        motor_start();
        // HAL_GPIO_WritePin(KEY3_GPIO_Port, KEY3_Pin, GPIO_PIN_RESET);
        // HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    }
    else if (motor_state_delay == 1) {
        // motor_end();
    }
}
