#include "lvgl_usr.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

void lvgl_init_ui()
{
    lv_init();
    lv_port_disp_init();                   // 注册LVGL的显示任务
    lv_port_indev_init();                  // 注册LVGL的触屏检测任务
}
