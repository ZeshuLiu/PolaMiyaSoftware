#ifndef __LVGL_USR_H__
#define __LVGL_USR_H__

#include "main.h"
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

void lvgl_init_ui(void);
void lvgl_update_display(void);
void lvgl_on_key(uint8_t key_index, uint8_t key_action);

/* 显示数据缓冲区 */
extern char lvgl_bat_vals[6];      // 电池电压 "-.--V"
extern char lvgl_key_vals[4];      // 按键状态 "x-x"
extern char lvgl_distance_vals[6]; // 距离 "xx.xx"
extern char lvgl_core_temp_vals[3];   // 核心温度 "xx"
extern char lvgl_board_temp_vals[3];  // 板载温度 "xx"
extern char lvgl_rbc_vals[6];      // RBC "00000"
extern char lvgl_mtl_vals[6];      // MTL "00000"
extern char lvgl_stc_vals[6];      // STC "00000"

#endif