#ifndef __LVGL_USR_H__
#define __LVGL_USR_H__

#include "main.h"

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