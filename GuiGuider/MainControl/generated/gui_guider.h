/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"


typedef struct
{
  
	lv_obj_t *main_cont;
	bool main_cont_del;
	lv_obj_t *MainFun;
	bool MainFun_del;
	lv_obj_t *MainFun_line_1;
	lv_obj_t *MainFun_label_Dist;
	lv_obj_t *MainFun_label_DistDash2;
	lv_obj_t *MainFun_label_DistDash3;
	lv_obj_t *MainFun_label_DistDash1;
	lv_obj_t *MainFun_label_DistDash4;
	lv_obj_t *MainFun_label_DistDash0;
	lv_obj_t *MainFun_line_2;
	lv_obj_t *MainFun_line_3;
	lv_obj_t *MainFun_BatBar;
	lv_obj_t *MainFun_line_4;
	lv_obj_t *MainFun_ProjName;
	lv_obj_t *MainFun_RunTime;
	lv_obj_t *MainFun_chart_1;
	lv_chart_series_t *MainFun_chart_1_0;
	lv_obj_t *MainFun_label_CahrtTitle;
	lv_obj_t *MainFun_label_CharTime4;
	lv_obj_t *MainFun_label_CharTime3;
	lv_obj_t *MainFun_label_CharTime2;
	lv_obj_t *MainFun_label_CharTime1;
	lv_obj_t *MainFun_label_CharTime0;
	lv_obj_t *MainFun_label_CharTLower;
	lv_obj_t *MainFun_label_ChartUpper;
	lv_obj_t *MainFun_label_ChartMiddle;
	lv_obj_t *MainFun_label_RBC;
	lv_obj_t *MainFun_label_MTL;
	lv_obj_t *MainFun_label_STC;
	lv_obj_t *MainFun_label_RBCVal;
	lv_obj_t *MainFun_label_MTLVal;
	lv_obj_t *MainFun_label_STCVal;
	lv_obj_t *MainFun_line_5;
	lv_obj_t *MainFun_line_6;
	lv_obj_t *MainFun_line_7;
	lv_obj_t *MainFun_line_8;
	lv_obj_t *MainFun_line_9;
	lv_obj_t *MainFun_line_10;
	lv_obj_t *MainFun_label_DistVal;
	lv_obj_t *MainFun_label_BatPercent;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_screen_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, uint32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                  uint32_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                  lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_completed_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_bottom_layer(void);

void setup_ui(lv_ui *ui);

void video_play(lv_ui *ui);

void init_keyboard(lv_ui *ui);

extern lv_ui guider_ui;


void setup_scr_main_cont(lv_ui *ui);
void setup_scr_MainFun(lv_ui *ui);

LV_FONT_DECLARE(lv_font_montserratMedium_12)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_montserratMedium_14)
LV_FONT_DECLARE(lv_font_Antonio_Regular_16)
LV_FONT_DECLARE(lv_font_montserratMedium_10)


#ifdef __cplusplus
}
#endif
#endif
