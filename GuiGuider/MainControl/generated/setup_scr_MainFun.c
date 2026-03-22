/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



int MainFun_RunTime_min_value = 0;
int MainFun_RunTime_hour_value = 0;
int MainFun_RunTime_sec_value = 0;
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
    lv_chart_set_range(ui->MainFun_chart_1, LV_CHART_AXIS_PRIMARY_Y, 0, 20);
    lv_chart_set_range(ui->MainFun_chart_1, LV_CHART_AXIS_SECONDARY_Y, 0, 100);
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
    lv_label_set_text(ui->MainFun_label_ChartUpper, "20");
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
    lv_label_set_text(ui->MainFun_label_ChartMiddle, "10");
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
    lv_obj_set_size(ui->MainFun_label_STC, 109, 13);
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
    lv_obj_set_size(ui->MainFun_label_STCVal, 40, 14);
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
    lv_label_set_text(ui->MainFun_label_BatPercent, "100%");
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
