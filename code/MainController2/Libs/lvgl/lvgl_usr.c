#include "lvgl_usr.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include <stdint.h>

#define lv_font_montserratMedium_16 lv_font_montserrat_16
#define lv_font_Antonio_Regular_16 lv_font_montserrat_16
#define lv_font_montserratMedium_14 lv_font_montserrat_14
#define lv_font_montserratMedium_12 lv_font_montserrat_12
#define lv_font_montserratMedium_10 lv_font_montserrat_10

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

/* 测距分度值设置 */
// "inf(20)-7m-5m-4m-3.5m-3m-2.5m-2m-1.7m-1.5m-1.3m-1.2m-1.05m-1m"
const float MeterGrid[18] = {-1,-1,1.0, 1.05, 1.2, 1.3, 1.5, 1.7, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 7.0, 20.0, 21, 21};

const char MeterGridChar[19][6] = {"", "", "1.0", "1.05", "1.2", "1.3", "1.5", "1.7", "2.0",
                                "2.5", "3.0", "3.5", "4.0", "5.0", "7.0", "INF","", "", "N/A"};

const int16_t MeterPosX[5][MeterPosResL] =
{{-14, -13, -11, -10, -8, -7, -5, -4, -2, -1, 1, 3, 4, 6, 7, 9, 11, 12, 14, 15, 17, 19, 20, 22, 24, 25, 27, 29},
    {30, 32, 34, 36, 37, 39, 41, 42, 44, 46, 48, 49, 51, 53, 55, 56, 58, 60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 78},
    {80, 81, 83, 85, 87, 89, 90, 92, 94, 96, 98, 100, 101, 103, 105, 107, 109, 110, 112, 114, 116, 118, 120, 121, 123, 125, 127, 129},
    {130, 132, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150, 152, 154, 155, 157, 159, 161, 162, 164, 166, 168, 169, 171, 173, 174, 176, 178},
    {180, 181, 183, 185, 186, 188, 190, 191, 193, 195, 196, 198, 199, 201, 203, 204, 206, 207, 209, 211, 212, 214, 215, 217, 218, 220, 221, 223}};

const int16_t MeterPosY[5][MeterPosResL] =
{{224, 223, 222, 221, 220, 219, 218, 217, 216, 215, 214, 213, 212, 211, 210, 210, 209, 208, 207, 206, 205, 205, 204, 203, 203, 202, 201, 200},
    {200, 199, 199, 198, 197, 197, 196, 196, 195, 195, 194, 194, 193, 193, 192, 192, 191, 191, 191, 190, 190, 189, 189, 189, 189, 188, 188, 188},
    {188, 187, 187, 187, 187, 187, 187, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 186, 187, 187, 187, 187, 187, 187},
    {188, 188, 188, 188, 189, 189, 189, 189, 190, 190, 191, 191, 191, 192, 192, 193, 193, 194, 194, 195, 195, 196, 196, 197, 197, 198, 199, 199},
    {200, 200, 201, 202, 203, 203, 204, 205, 205, 206, 207, 208, 209, 210, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223}};

/* 第二界面对象引用 - MainFun */
static lv_obj_t *screen2_cont = NULL;
static lv_ui ui2;
static lv_obj_t *MainFun_line_1 = NULL;
static lv_obj_t *MainFun_label_Dist = NULL;
static lv_obj_t *MainFun_label_DistVal = NULL;
static lv_obj_t *MainFun_line_2 = NULL;
static lv_obj_t *MainFun_BatBar = NULL;
static lv_obj_t *MainFun_line_4 = NULL;
static lv_obj_t *MainFun_ProjName = NULL;
static lv_obj_t *MainFun_RunTime = NULL;
// static lv_obj_t *MainFun_chart_1 = NULL;
static lv_obj_t *MainFun_chart_series_1 = NULL;
static lv_obj_t *MainFun_label_BatTitle = NULL;
static lv_obj_t *MainFun_label_BatVal = NULL;

/* 显示数据缓冲区 */
char lvgl_bat_vals[6] = "-.--V";
char lvgl_key_vals[4] = "x-x";
char lvgl_distance_vals[6] = "xx.xx";
char lvgl_core_temp_vals[3] = "xx";
char lvgl_board_temp_vals[3] = "xx";
char lvgl_batpercent_vals[5] = "xxxx";
char lvgl_rbc_vals[6] = "00000";
char lvgl_mtl_vals[6] = "00000";
char lvgl_stc_vals[6] = "00000";

int MainFun_RunTime_min_value = 0;
int MainFun_RunTime_hour_value = 0;
int MainFun_RunTime_sec_value = 0;


/* UI 对象引用 */
static lv_obj_t *main_cont = NULL;  // 主容器
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

void setup_scr_MainFun(lv_ui *ui);

/* 当前界面索引 (0=主界面，1=第二界面) */
static uint8_t current_screen = 0;

/* 运行时间计时器 */
static uint32_t runtime_seconds = 0;
static void MainFun_RunTime_timer(lv_timer_t * timer)
{
    runtime_seconds++;
    uint32_t hours = runtime_seconds / 3600;
    uint32_t minutes = (runtime_seconds % 3600) / 60;
    uint32_t secs = runtime_seconds % 60;

    if (MainFun_RunTime) {
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%d:%02d:%02d", hours, minutes, secs);
        lv_label_set_text(MainFun_RunTime, buf);
    }
}

float calc_bat_percent(float v)
{
    if (v >= 4.1) return 100;
    if (v >= 4.0) return 85 + (v - 4.0) * 150;   // 快掉区
    if (v >= 3.8) return 60 + (v - 3.8) * 125;
    if (v >= 3.7) return 40 + (v - 3.7) * 200;  // 平台
    if (v >= 3.6) return 25 + (v - 3.6) * 150;
    if (v >= 3.5) return (v - 3.5) * 250;


    return 0;
}

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
    main_cont = lv_obj_create(NULL);
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

    /* ========== 创建第二界面 (MainFun) ========== */
    setup_scr_MainFun(&ui2);

    lv_screen_load(ui2.MainFun);

    /* 启动运行时间计时器 */
    // lv_timer_create(MainFun_RunTime_timer, 1000, NULL);
}


void setup_scr_MainFun(lv_ui *ui)
{
    //Write codes MainFun
    ui->MainFun = lv_obj_create(NULL);
    lv_obj_set_size(ui->MainFun, 280, 240);
    lv_obj_set_scrollbar_mode(ui->MainFun, LV_SCROLLBAR_MODE_OFF);

    //Write style for MainFun, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->MainFun, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->MainFun, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->MainFun, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_1
    ui->MainFun_line_1 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_1, 240, 30);
    lv_obj_set_size(ui->MainFun_line_1, 1, 210);
    static lv_point_precise_t MainFun_line_1[] = {{0, 0},{0, 210}};
    lv_line_set_points(ui->MainFun_line_1, MainFun_line_1, 2);

    //Write style for MainFun_line_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_1, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_Dist
    ui->MainFun_label_Dist = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_Dist, 66, 213);
    lv_obj_set_size(ui->MainFun_label_Dist, 76, 15);
    lv_label_set_text(ui->MainFun_label_Dist, "Distance:    ");
    lv_label_set_long_mode(ui->MainFun_label_Dist, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_Dist, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_Dist, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_Dist, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_Dist, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_Dist, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_Dist, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_Dist, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->MainFun_label_Dist, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->MainFun_label_Dist, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_Dist, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_DistDash2
    ui->MainFun_label_DistDash2 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_DistDash2, 105, 186);
    lv_obj_set_size(ui->MainFun_label_DistDash2, 30, 16);
    lv_label_set_text(ui->MainFun_label_DistDash2, "2.5");
    lv_label_set_long_mode(ui->MainFun_label_DistDash2, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_DistDash2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_DistDash2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_DistDash2, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_DistDash2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_DistDash2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_DistDash2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_DistDash3
    ui->MainFun_label_DistDash3 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_DistDash3, 150, 191);
    lv_obj_set_size(ui->MainFun_label_DistDash3, 30, 16);
    lv_label_set_text(ui->MainFun_label_DistDash3, "2.0");
    lv_label_set_long_mode(ui->MainFun_label_DistDash3, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_DistDash3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_DistDash3, lv_color_hex(0xc8c8c8), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_DistDash3, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_DistDash3, 200, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_DistDash3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_DistDash3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_DistDash1
    ui->MainFun_label_DistDash1 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_DistDash1, 60, 191);
    lv_obj_set_size(ui->MainFun_label_DistDash1, 30, 16);
    lv_label_set_text(ui->MainFun_label_DistDash1, "3");
    lv_label_set_long_mode(ui->MainFun_label_DistDash1, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_DistDash1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_DistDash1, lv_color_hex(0xc8c8c8), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_DistDash1, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_DistDash1, 200, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_DistDash1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_DistDash1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_DistDash4
    ui->MainFun_label_DistDash4 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_DistDash4, 187, 202);
    lv_obj_set_size(ui->MainFun_label_DistDash4, 30, 16);
    lv_label_set_text(ui->MainFun_label_DistDash4, "1.7");
    lv_label_set_long_mode(ui->MainFun_label_DistDash4, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_DistDash4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_DistDash4, lv_color_hex(0x969696), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_DistDash4, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_DistDash4, 150, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_DistDash4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_DistDash4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_DistDash0
    ui->MainFun_label_DistDash0 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_DistDash0, 23, 202);
    lv_obj_set_size(ui->MainFun_label_DistDash0, 30, 16);
    lv_label_set_text(ui->MainFun_label_DistDash0, "3.5");
    lv_label_set_long_mode(ui->MainFun_label_DistDash0, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_DistDash0, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_DistDash0, lv_color_hex(0x969696), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_DistDash0, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_DistDash0, 150, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_DistDash0, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_DistDash0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_2
    ui->MainFun_line_2 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_2, 120, 205);
    lv_obj_set_size(ui->MainFun_line_2, 3, 5);
    static lv_point_precise_t MainFun_line_2[] = {{0, 0},{0, 6}};
    lv_line_set_points(ui->MainFun_line_2, MainFun_line_2, 2);

    //Write style for MainFun_line_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_2, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_2, lv_color_hex(0xb1ffd3), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_3
    ui->MainFun_line_3 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_3, 120, 184);
    lv_obj_set_size(ui->MainFun_line_3, 9, 12);
    static lv_point_precise_t MainFun_line_3[] = {{0, 0},{0, 2}};
    lv_line_set_points(ui->MainFun_line_3, MainFun_line_3, 2);

    //Write style for MainFun_line_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_3, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_3, lv_color_hex(0xb1ffd3), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_BatBar
    ui->MainFun_BatBar = lv_bar_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_BatBar, 205, 11);
    lv_obj_set_size(ui->MainFun_BatBar, 60, 14);
    lv_obj_set_style_anim_duration(ui->MainFun_BatBar, 1000, 0);
    lv_bar_set_mode(ui->MainFun_BatBar, LV_BAR_MODE_NORMAL);
    lv_bar_set_range(ui->MainFun_BatBar, 0, 100);
    lv_bar_set_value(ui->MainFun_BatBar, 50, LV_ANIM_OFF);

    //Write style for MainFun_BatBar, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->MainFun_BatBar, 171, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->MainFun_BatBar, lv_color_hex(0xe0e0e0), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->MainFun_BatBar, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_BatBar, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_BatBar, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for MainFun_BatBar, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->MainFun_BatBar, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->MainFun_BatBar, lv_color_hex(0x00ff00), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->MainFun_BatBar, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_BatBar, 10, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes MainFun_line_4
    ui->MainFun_line_4 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_4, 0, 30);
    lv_obj_set_size(ui->MainFun_line_4, 280, 2);
    static lv_point_precise_t MainFun_line_4[] = {{0, 0},{280, 0}};
    lv_line_set_points(ui->MainFun_line_4, MainFun_line_4, 2);

    //Write style for MainFun_line_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_4, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_4, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_ProjName
    ui->MainFun_ProjName = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_ProjName, 38, 12);
    lv_obj_set_size(ui->MainFun_ProjName, 75, 18);
    lv_label_set_text(ui->MainFun_ProjName, "PolaMiya ");
    lv_label_set_long_mode(ui->MainFun_ProjName, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_ProjName, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_ProjName, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_ProjName, &lv_font_montserratMedium_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_ProjName, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_ProjName, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->MainFun_ProjName, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_RunTime
    static bool MainFun_RunTime_timer_enabled = false;
    ui->MainFun_RunTime = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_RunTime, 121, 13);
    lv_obj_set_size(ui->MainFun_RunTime, 72, 18);
    lv_label_set_text(ui->MainFun_RunTime, "0:00:00");
    if (!MainFun_RunTime_timer_enabled) {
        lv_timer_create(MainFun_RunTime_timer, 1000, NULL);
        MainFun_RunTime_timer_enabled = true;
    }

    //Write style for MainFun_RunTime, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_RunTime, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_RunTime, &lv_font_Antonio_Regular_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_RunTime, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_RunTime, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_RunTime, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_RunTime, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_chart_1
    ui->MainFun_chart_1 = lv_chart_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_chart_1, 33, 112);
    lv_obj_set_size(ui->MainFun_chart_1, 201, 48);
    lv_obj_set_scrollbar_mode(ui->MainFun_chart_1, LV_SCROLLBAR_MODE_OFF);
    lv_chart_set_type(ui->MainFun_chart_1, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(ui->MainFun_chart_1, 3, 5);
    lv_chart_set_point_count(ui->MainFun_chart_1, 5);
    lv_chart_set_range(ui->MainFun_chart_1, LV_CHART_AXIS_PRIMARY_Y, 0, 40);
    lv_chart_set_range(ui->MainFun_chart_1, LV_CHART_AXIS_SECONDARY_Y, 0, 40);
    lv_obj_set_style_size(ui->MainFun_chart_1, 0, 0, LV_PART_INDICATOR);
    ui->MainFun_chart_1_0 = lv_chart_add_series(ui->MainFun_chart_1, lv_color_hex(0xffffff), LV_CHART_AXIS_PRIMARY_Y);
#if LV_USE_FREEMASTER == 0
    lv_chart_set_next_value(ui->MainFun_chart_1, ui->MainFun_chart_1_0, 1);
    lv_chart_set_next_value(ui->MainFun_chart_1, ui->MainFun_chart_1_0, 4);
    lv_chart_set_next_value(ui->MainFun_chart_1, ui->MainFun_chart_1_0, 2);
    lv_chart_set_next_value(ui->MainFun_chart_1, ui->MainFun_chart_1_0, 14);
    lv_chart_set_next_value(ui->MainFun_chart_1, ui->MainFun_chart_1_0, 1);
#endif

    //Write style for MainFun_chart_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_chart_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->MainFun_chart_1, 198, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->MainFun_chart_1, lv_color_hex(0xe8e8e8), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->MainFun_chart_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_chart_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_chart_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->MainFun_chart_1, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->MainFun_chart_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_chart_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(ui->MainFun_chart_1, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_chart_1, lv_color_hex(0xe8e8e8), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_chart_1, 73, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CahrtTitle
    ui->MainFun_label_CahrtTitle = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CahrtTitle, 79, 94);
    lv_obj_set_size(ui->MainFun_label_CahrtTitle, 97, 14);
    lv_label_set_text(ui->MainFun_label_CahrtTitle, "Temperature");
    lv_label_set_long_mode(ui->MainFun_label_CahrtTitle, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CahrtTitle, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CahrtTitle, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CahrtTitle, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CahrtTitle, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CahrtTitle, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CahrtTitle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CharTime4
    ui->MainFun_label_CharTime4 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CharTime4, 207, 164);
    lv_obj_set_size(ui->MainFun_label_CharTime4, 22, 10);
    lv_label_set_text(ui->MainFun_label_CharTime4, "-10s");
    lv_label_set_long_mode(ui->MainFun_label_CharTime4, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CharTime4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CharTime4, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CharTime4, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CharTime4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CharTime4, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CharTime4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CharTime3
    ui->MainFun_label_CharTime3 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CharTime3, 163, 164);
    lv_obj_set_size(ui->MainFun_label_CharTime3, 22, 10);
    lv_label_set_text(ui->MainFun_label_CharTime3, "-20s");
    lv_label_set_long_mode(ui->MainFun_label_CharTime3, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CharTime3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CharTime3, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CharTime3, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CharTime3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CharTime3, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CharTime3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CharTime2
    ui->MainFun_label_CharTime2 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CharTime2, 121, 164);
    lv_obj_set_size(ui->MainFun_label_CharTime2, 22, 10);
    lv_label_set_text(ui->MainFun_label_CharTime2, "-30s");
    lv_label_set_long_mode(ui->MainFun_label_CharTime2, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CharTime2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CharTime2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CharTime2, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CharTime2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CharTime2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CharTime2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CharTime1
    ui->MainFun_label_CharTime1 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CharTime1, 72, 164);
    lv_obj_set_size(ui->MainFun_label_CharTime1, 31, 10);
    lv_label_set_text(ui->MainFun_label_CharTime1, "-40s");
    lv_label_set_long_mode(ui->MainFun_label_CharTime1, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CharTime1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CharTime1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CharTime1, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CharTime1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CharTime1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CharTime1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CharTime0
    ui->MainFun_label_CharTime0 = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CharTime0, 29, 164);
    lv_obj_set_size(ui->MainFun_label_CharTime0, 31, 10);
    lv_label_set_text(ui->MainFun_label_CharTime0, "-50s");
    lv_label_set_long_mode(ui->MainFun_label_CharTime0, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CharTime0, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CharTime0, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CharTime0, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CharTime0, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CharTime0, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CharTime0, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_CharTLower
    ui->MainFun_label_CharTLower = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_CharTLower, 8, 146);
    lv_obj_set_size(ui->MainFun_label_CharTLower, 22, 10);
    lv_label_set_text(ui->MainFun_label_CharTLower, "0");
    lv_label_set_long_mode(ui->MainFun_label_CharTLower, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_CharTLower, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_CharTLower, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_CharTLower, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_CharTLower, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_CharTLower, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_CharTLower, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_ChartUpper
    ui->MainFun_label_ChartUpper = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_ChartUpper, 8, 116);
    lv_obj_set_size(ui->MainFun_label_ChartUpper, 22, 10);
    lv_label_set_text(ui->MainFun_label_ChartUpper, "40");
    lv_label_set_long_mode(ui->MainFun_label_ChartUpper, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_ChartUpper, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_ChartUpper, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_ChartUpper, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_ChartUpper, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_ChartUpper, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_ChartUpper, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_ChartMiddle
    ui->MainFun_label_ChartMiddle = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_ChartMiddle, 8, 131);
    lv_obj_set_size(ui->MainFun_label_ChartMiddle, 22, 10);
    lv_label_set_text(ui->MainFun_label_ChartMiddle, "20");
    lv_label_set_long_mode(ui->MainFun_label_ChartMiddle, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_ChartMiddle, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_ChartMiddle, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_ChartMiddle, &lv_font_montserratMedium_10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_ChartMiddle, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_ChartMiddle, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_ChartMiddle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_RBC
    ui->MainFun_label_RBC = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_RBC, 23, 35);
    lv_obj_set_size(ui->MainFun_label_RBC, 106, 14);
    lv_label_set_text(ui->MainFun_label_RBC, "Reboot Count:");
    lv_label_set_long_mode(ui->MainFun_label_RBC, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_RBC, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_RBC, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_RBC, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_RBC, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_RBC, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_RBC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_MTL
    ui->MainFun_label_MTL = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_MTL, 23, 53);
    lv_obj_set_size(ui->MainFun_label_MTL, 103, 14);
    lv_label_set_text(ui->MainFun_label_MTL, "Motor Life:");
    lv_label_set_long_mode(ui->MainFun_label_MTL, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_MTL, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_MTL, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_MTL, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_MTL, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_MTL, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_MTL, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_STC
    ui->MainFun_label_STC = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_STC, 23, 71);
    lv_obj_set_size(ui->MainFun_label_STC, 109, 14);
    lv_label_set_text(ui->MainFun_label_STC, "Shutter Count:");
    lv_label_set_long_mode(ui->MainFun_label_STC, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_STC, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_STC, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_STC, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_STC, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_STC, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_STC, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_Val
    ui->MainFun_label_RBCVal = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_RBCVal, 129, 35);
    lv_obj_set_size(ui->MainFun_label_RBCVal, 60, 14);
    lv_label_set_text(ui->MainFun_label_RBCVal, "0");
    lv_label_set_long_mode(ui->MainFun_label_RBCVal, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_Val, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_RBCVal, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_RBCVal, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_RBCVal, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_RBCVal, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_RBCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_MTLVal
    ui->MainFun_label_MTLVal = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_MTLVal, 101, 53);
    lv_obj_set_size(ui->MainFun_label_MTLVal, 60, 14);
    lv_label_set_text(ui->MainFun_label_MTLVal, "0");
    lv_label_set_long_mode(ui->MainFun_label_MTLVal, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_MTLVal, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_MTLVal, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_MTLVal, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_MTLVal, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_MTLVal, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_MTLVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_STCVal
    ui->MainFun_label_STCVal = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_STCVal, 129, 71);
    lv_obj_set_size(ui->MainFun_label_STCVal, 60, 14);
    lv_label_set_text(ui->MainFun_label_STCVal, "0");
    lv_label_set_long_mode(ui->MainFun_label_STCVal, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_STCVal, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_STCVal, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_STCVal, &lv_font_montserratMedium_14, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_STCVal, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_STCVal, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_STCVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_5
    ui->MainFun_line_5 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_5, 0, 88);
    lv_obj_set_size(ui->MainFun_line_5, 280, 1);
    static lv_point_precise_t MainFun_line_5[] = {{0, 0},{240, 0}};
    lv_line_set_points(ui->MainFun_line_5, MainFun_line_5, 2);

    //Write style for MainFun_line_5, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_5, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_5, lv_color_hex(0x757575), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_5, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_5, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_6
    ui->MainFun_line_6 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_6, 9, 7);
    lv_obj_set_size(ui->MainFun_line_6, 33, 10);
    static lv_point_precise_t MainFun_line_6[] = {{0, 0},{20, 0}};
    lv_line_set_points(ui->MainFun_line_6, MainFun_line_6, 2);

    //Write style for MainFun_line_6, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_6, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_6, lv_color_hex(0xff0027), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_6, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_6, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_7
    ui->MainFun_line_7 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_7, 9, 12);
    lv_obj_set_size(ui->MainFun_line_7, 33, 10);
    static lv_point_precise_t MainFun_line_7[] = {{0, 0},{20, 0}};
    lv_line_set_points(ui->MainFun_line_7, MainFun_line_7, 2);

    //Write style for MainFun_line_7, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_7, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_7, lv_color_hex(0xff6500), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_7, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_7, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_8
    ui->MainFun_line_8 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_8, 9, 17);
    lv_obj_set_size(ui->MainFun_line_8, 33, 10);
    static lv_point_precise_t MainFun_line_8[] = {{0, 0},{20, 0}};
    lv_line_set_points(ui->MainFun_line_8, MainFun_line_8, 2);

    //Write style for MainFun_line_8, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_8, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_8, lv_color_hex(0xffbf00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_8, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_8, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_9
    ui->MainFun_line_9 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_9, 9, 23);
    lv_obj_set_size(ui->MainFun_line_9, 33, 10);
    static lv_point_precise_t MainFun_line_9[] = {{0, 0},{20, 0}};
    lv_line_set_points(ui->MainFun_line_9, MainFun_line_9, 2);

    //Write style for MainFun_line_9, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_9, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_9, lv_color_hex(0x5aff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_9, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_9, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_line_10
    ui->MainFun_line_10 = lv_line_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_line_10, 9, 28);
    lv_obj_set_size(ui->MainFun_line_10, 33, 10);
    static lv_point_precise_t MainFun_line_10[] = {{0, 0},{20, 0}};
    lv_line_set_points(ui->MainFun_line_10, MainFun_line_10, 2);

    //Write style for MainFun_line_10, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_line_width(ui->MainFun_line_10, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui->MainFun_line_10, lv_color_hex(0x0063ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui->MainFun_line_10, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_rounded(ui->MainFun_line_10, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_DistVal
    ui->MainFun_label_DistVal = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_DistVal, 129, 213);
    lv_obj_set_size(ui->MainFun_label_DistVal, 43, 15);
    lv_label_set_text(ui->MainFun_label_DistVal, "2.5 m");
    lv_label_set_long_mode(ui->MainFun_label_DistVal, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_DistVal, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_DistVal, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_DistVal, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_DistVal, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_DistVal, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_DistVal, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_DistVal, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->MainFun_label_DistVal, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->MainFun_label_DistVal, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_DistVal, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes MainFun_label_BatPercent
    ui->MainFun_label_BatPercent = lv_label_create(ui->MainFun);
    lv_obj_set_pos(ui->MainFun_label_BatPercent, 211, 12);
    lv_obj_set_size(ui->MainFun_label_BatPercent, 50, 12);
    lv_label_set_text(ui->MainFun_label_BatPercent, "0%");
    lv_label_set_long_mode(ui->MainFun_label_BatPercent, LV_LABEL_LONG_WRAP);

    //Write style for MainFun_label_BatPercent, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->MainFun_label_BatPercent, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->MainFun_label_BatPercent, &lv_font_montserratMedium_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->MainFun_label_BatPercent, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->MainFun_label_BatPercent, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->MainFun_label_BatPercent, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    //The custom code of MainFun.


    //Update current screen layout.
    lv_obj_update_layout(ui->MainFun);

}

/* 更新测距刻度标签 - 根据实测距离动态更新 5 个标签的内容、位置和样式 */
static void update_meter_labels(float distance)
{
    /* 标签对象数组 */
    lv_obj_t *dist_labels[5] = {
        ui2.MainFun_label_DistDash0,
        ui2.MainFun_label_DistDash1,
        ui2.MainFun_label_DistDash2,
        ui2.MainFun_label_DistDash3,
        ui2.MainFun_label_DistDash4
    };

    /* 处理异常情况，测距小于0 */
    if (distance < 0.0f) {
        /* 显示异常，所有标签显示 N/A，使用中心位置 */
        for (int i = 0; i < 5; i++) {
            if (dist_labels[i]) {
                lv_label_set_text(dist_labels[i], "");
                /* 使用分辨率索引 14 作为默认位置 */
                lv_obj_set_pos(dist_labels[i], MeterPosX[i][14], MeterPosY[i][14]);
                lv_obj_set_style_text_opa(dist_labels[i], 150, LV_PART_MAIN|LV_STATE_DEFAULT);
            }
        }
        lv_label_set_text(dist_labels[2], MeterGridChar[18]);
        return;
    }

    /* 测距大于20则认为处于无穷远 */
    if (distance >= 20.0f) {
        /* 显示异常，所有标签显示 N/A，使用中心位置 */
        for (int i = 0; i < 5; i++) {
            if (dist_labels[i]) {
                lv_label_set_text(dist_labels[i], MeterGridChar[15+i]);
                /* 使用分辨率索引 14 作为默认位置 */
                lv_obj_set_pos(dist_labels[i], MeterPosX[i][14], MeterPosY[i][14]);
                lv_obj_set_style_text_opa(dist_labels[i], (MeterPosX[i][14]>105)? (255-(MeterPosX[i][14]-105)) : (255-(105-MeterPosX[i][14])), LV_PART_MAIN|LV_STATE_DEFAULT);
            }
        }
        return;
    }

    /* 判断此时的距离位于哪两个标度之间 */
    uint8_t dis_upper = 0;
    uint8_t dis_bias = 0;
    for (dis_upper = 2; dis_upper < 16; dis_upper++)
    {
        if (distance < MeterGrid[dis_upper]){
            dis_bias = (int) ((distance - MeterGrid[dis_upper-1])*28.0f/(MeterGrid[dis_upper] - MeterGrid[dis_upper-1]));
            break;
        }
    }
    if (dis_bias<14){
        dis_upper -= 1;
        dis_bias = 14-dis_bias;
        // dis_bias += 14;
    }
    else{
        dis_bias = 41-dis_bias;
    }
    if (dis_bias < 0) dis_bias = 0;
    if (dis_bias > 27) dis_bias = 27;

    for (int i = 0; i < 5; i++) {
        if (dist_labels[i]) {
            lv_label_set_text(dist_labels[i], MeterGridChar[dis_upper+i-2]);
            /* 使用分辨率索引 14 作为默认位置 */
            lv_obj_set_pos(dist_labels[i], MeterPosX[i][dis_bias], MeterPosY[i][dis_bias]);
            lv_obj_set_style_text_opa(dist_labels[i], (MeterPosX[i][dis_bias]>105)? (255-(MeterPosX[i][dis_bias]-105)) : (255-(105-MeterPosX[i][dis_bias])), LV_PART_MAIN|LV_STATE_DEFAULT);
        }
    }
}


/* 更新显示内容 - 需要在主循环中调用 */
void lvgl_update_display(void)
{
    /* ========== 根据当前屏幕更新对应界面 ========== */
    if (current_screen == 0) {
        /* ----- 更新主界面 ----- */
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

    /* ========== 更新第二界面 ========== */
    if (current_screen == 1 || ui2.MainFun) {
        /* 更新电池电量 Bar */
        if (ui2.MainFun_BatBar && BAT_V > -0.1f && BAT_V < 10.0f) {
            // int bat_percent = (int)((BAT_V - 3.0) / 1.2 * 100);  // 3.0V-4.2V 映射到 0-100%
            int bat_percent = (int)calc_bat_percent(BAT_V);
            if (bat_percent < 0) bat_percent = 0;
            if (bat_percent > 100) bat_percent = 100;
            lv_bar_set_value(ui2.MainFun_BatBar, bat_percent, LV_ANIM_OFF);
            /* 根据电量改变颜色 */
            if (bat_percent < 20) {
                lv_obj_set_style_bg_color(ui2.MainFun_BatBar, lv_color_hex(0xff0000), LV_PART_INDICATOR|LV_STATE_DEFAULT);
            } else if (bat_percent < 50) {
                lv_obj_set_style_bg_color(ui2.MainFun_BatBar, lv_color_hex(0xffff00), LV_PART_INDICATOR|LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_bg_color(ui2.MainFun_BatBar, lv_color_hex(0x00ff00), LV_PART_INDICATOR|LV_STATE_DEFAULT);
            }

            /* 更新电量百分比显示 (0-100%) */
            lvgl_batpercent_vals[0] = (bat_percent / 100) + '0';
            lvgl_batpercent_vals[1] = ((bat_percent % 100) / 10) + '0';
            lvgl_batpercent_vals[2] = (bat_percent % 10) + '0';
            lvgl_batpercent_vals[3] = '%';
            lvgl_batpercent_vals[4] = '\0';
            if (ui2.MainFun_label_BatPercent) {
                lv_label_set_text(ui2.MainFun_label_BatPercent, lvgl_batpercent_vals);
            }
        }

        /* 更新距离显示 */
        if (ui2.MainFun_label_DistVal && distance >= 0 && distance < 20.0f) {
            if (distance >= 0 && distance < 20.0f) {
                lv_label_set_float(ui2.MainFun_label_DistVal, distance);
                lv_strcpy(lvgl_distance_vals, lv_label_get_text(ui2.MainFun_label_DistVal));
            }
            else if (distance >= 20.0f){
                lv_label_set_text(ui2.MainFun_label_DistVal, "Infinity");
                lv_strcpy(lvgl_distance_vals, "Infinity");
            }
            else {
                lv_label_set_text(ui2.MainFun_label_DistVal, "xx.xx");
                lv_strcpy(lvgl_distance_vals, "xx.xx");
            }
        }

        /* 更新测距刻度标签 (动态位置) */
        update_meter_labels(distance);

        /* 更新温度图表 */
        if (ui2.MainFun_chart_1) {
            static int temp_history[5] = {0, 0, 0, 0, 0};
            static int temp_idx = 0;

            /* 添加新温度值 */
            int temp_val = (int)(tmp102Temp);
            temp_history[temp_idx] = temp_val;
            temp_idx = (temp_idx + 1) % 5;

            /* 更新图表数据 */
            lv_chart_set_next_value(ui2.MainFun_chart_1, ui2.MainFun_chart_1_0, temp_val);
        }

        /* RBC/MTL/STC: 从 EEPROM 数据同步 */
        if (ui2.MainFun_label_RBCVal) lv_label_set_text(ui2.MainFun_label_RBCVal, RBC_vals);
        if (ui2.MainFun_label_MTLVal) lv_label_set_text(ui2.MainFun_label_MTLVal, MTL_vals);
        if (ui2.MainFun_label_STCVal) lv_label_set_text(ui2.MainFun_label_STCVal, STC_vals);
    }
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

    /* KEY2 短按切换界面 */
    if (key_index == 2 && key_action == 2) {
        current_screen = 1 - current_screen;  // 切换界面
        if (current_screen == 1) {
            /* 加载主界面 */
            lv_screen_load(main_cont);
        } else {
            /* 加载第二界面 */
            lv_screen_load(ui2.MainFun);
        }
        return;
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
