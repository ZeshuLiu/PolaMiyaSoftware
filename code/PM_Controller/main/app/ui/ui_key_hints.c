#include "ui_key_hints.h"
#include "ui_home_assets.h"

#include <stdbool.h>

#define INK 0x09291e
#define GREEN 0x79f5a7
#define PALE 0xd4ffe7
#define AMBER 0xffbe58
#define CANDIDATE 0x386b4b

enum { FUNC_SETTINGS, FUNC_FOCUS, FUNC_EJECT, FUNC_INFO, FUNC_COUNT };
static lv_obj_t *rail, *top_key, *bottom_key, *middle;
static lv_obj_t *icons[3][FUNC_COUNT], *up, *down, *left, *right, *plus, *minus, *done, *stop;
static lv_obj_t *menu_enter, *menu_play, *menu_busy;
static int last_mode = -1;
static unsigned last_func;
static bool last_edit;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    if (color) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(obj, radius, 0);
    }
    return obj;
}

static void show(lv_obj_t *obj, bool visible)
{
    lv_obj_set_flag(obj, LV_OBJ_FLAG_HIDDEN, !visible);
}

static lv_obj_t *line(lv_obj_t *parent, const lv_point_precise_t *points, unsigned n)
{
    lv_obj_t *obj = lv_line_create(parent);
    lv_obj_remove_style_all(obj);
    lv_line_set_points(obj, points, n);
    lv_obj_set_style_line_color(obj, lv_color_hex(INK), 0);
    lv_obj_set_style_line_width(obj, 3, 0);
    lv_obj_set_style_line_rounded(obj, true, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

void ui_key_hints_create(lv_obj_t *screen)
{
    rail = box(screen, 136, 32, 36, 274, PALE, 4);
    top_key = box(rail, 4, 6, 28, 34, 0x9ce8ba, 3);
    bottom_key = box(rail, 4, 218, 28, 34, 0x9ce8ba, 3);
    box(rail, 6, 52, 24, 1, 0x78ad8a, 0);
    box(rail, 6, 212, 24, 1, 0x78ad8a, 0);
    middle = box(rail, 3, 110, 30, 44, INK, 3);
    static const lv_point_precise_t up_points[] = {{7,20},{14,13},{21,20}};
    static const lv_point_precise_t down_points[] = {{7,13},{14,20},{21,13}};
    static const lv_point_precise_t right_points[] = {{10,10},{17,17},{10,24}};
    static const lv_point_precise_t left_points[] = {{18,10},{11,17},{18,24}};
    static const lv_point_precise_t h[] = {{7,17},{21,17}};
    static const lv_point_precise_t v[] = {{14,10},{14,24}};
    static const lv_point_precise_t check[] = {{6,21},{13,28},{24,15}};
    up=line(top_key,up_points,3); down=line(bottom_key,down_points,3);
    right=line(top_key,right_points,3); left=line(bottom_key,left_points,3);
    plus=box(top_key,0,0,28,34,0,0); line(plus,h,2); line(plus,v,2);
    minus=line(bottom_key,h,2); done=line(middle,check,3);
    static const lv_point_precise_t enter_points[] = {{10,13},{19,22},{10,31}};
    static const lv_point_precise_t play_points[] = {{10,13},{20,22},{10,31},{10,13}};
    menu_enter=line(middle,enter_points,3);
    menu_play=line(middle,play_points,4);
    lv_obj_set_style_line_color(menu_enter,lv_color_hex(GREEN),0);
    lv_obj_set_style_line_color(menu_play,lv_color_hex(GREEN),0);
    menu_busy=box(middle,0,0,30,44,0,0);
    for(unsigned i=0;i<3;++i) box(menu_busy,6+(int)i*7,20,4,4,0x9ce8ba,LV_RADIUS_CIRCLE);
    const lv_image_dsc_t *images[] = {&ui_icon_gear_current, &ui_icon_focus_current,
                                       &ui_icon_eject_current, &ui_icon_info_current};
    /* Center the three-icon group between separators y=84/244.
     * Icon centers are y=120/164/208, 44 px apart. */
    const int ys[] = {77, 121, 165};
    for (unsigned slot=0; slot<3; ++slot) {
        for (unsigned f=0; f<FUNC_COUNT; ++f) {
            lv_obj_t *img=lv_image_create(rail);
            lv_obj_remove_style_all(img);
            lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_pos(img, 7, ys[slot]);
            lv_image_set_src(img, images[f]);
            lv_obj_set_style_image_recolor(img, lv_color_hex(slot==1 ? GREEN : CANDIDATE), 0);
            lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
            icons[slot][f]=img;
        }
    }
    /* EJECT has its own narrow rail and a stop control at KEY2. */
    stop=box(screen,159,155,10,10,0,1);
    lv_obj_set_style_border_color(stop,lv_color_hex(INK),0);
    lv_obj_set_style_border_width(stop,2,0);
    last_mode=-1;
}

static void reset(void)
{
    show(rail,true); show(stop,false);
    show(up,false); show(down,false); show(left,false); show(right,false);
    show(plus,false); show(minus,false); show(done,false);
    show(menu_enter,false); show(menu_play,false); show(menu_busy,false);
    for(unsigned i=0;i<3;++i) for(unsigned f=0;f<FUNC_COUNT;++f) show(icons[i][f],false);
}

static void center(unsigned func, bool editing)
{
    lv_obj_set_style_bg_color(middle,lv_color_hex(editing ? AMBER : INK),0);
    lv_obj_set_style_line_color(done,lv_color_hex(INK),0);
    if(editing) show(done,true); else show(icons[1][func],true);
}

void ui_key_hints_home(unsigned current, bool editing)
{
    if(current>=FUNC_COUNT) current=FUNC_FOCUS;
    if(last_mode==0 && last_func==current && last_edit==editing) return;
    last_mode=0; last_func=current; last_edit=editing;
    reset();
    center(current,editing);
    if(editing) { show(plus,true); show(minus,true); }
    else {
        show(up,true); show(down,true);
        show(icons[0][(current+FUNC_COUNT-1)%FUNC_COUNT],true);
        show(icons[2][(current+1)%FUNC_COUNT],true);
    }
}

void ui_key_hints_info(void)
{
    if(last_mode==1) return;
    last_mode=1; reset();
    center(FUNC_SETTINGS,false);
    show(right,true); show(left,true);
}

void ui_key_hints_eject(void)
{
    if(last_mode==4) return;
    last_mode=4; show(rail,false); show(stop,true);
}

void ui_key_hints_menu(ui_menu_hint_t hint)
{
    const int mode=5+(int)hint;
    if(last_mode==mode) return;
    last_mode=mode; reset();
    const bool editing=hint==UI_MENU_HINT_NUMBER || hint==UI_MENU_HINT_CHOICE;
    const bool disabled=hint==UI_MENU_HINT_BUSY || hint==UI_MENU_HINT_DISABLED;
    lv_obj_set_style_bg_color(middle,lv_color_hex(disabled ? 0x365148 : editing ? AMBER : INK),0);
    if(hint==UI_MENU_HINT_NUMBER) { show(plus,true); show(minus,true); }
    else { show(up,true); show(down,true); }
    if(disabled) show(menu_busy,true);
    else if(hint==UI_MENU_HINT_ACTION) show(menu_play,true);
    else if(hint==UI_MENU_HINT_OPEN) show(menu_enter,true);
    else {
        lv_obj_set_style_line_color(done,lv_color_hex(editing ? INK : GREEN),0);
        show(done,true);
    }
}
