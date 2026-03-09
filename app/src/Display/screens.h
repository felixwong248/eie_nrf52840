#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_LIST_PAGE = 1,
    _SCREEN_ID_LAST = 1
};

typedef struct _objects_t {
    lv_obj_t *list_page;
    lv_obj_t *title;
    lv_obj_t *obj0;
    lv_obj_t *songname1;
    lv_obj_t *songname2;
    lv_obj_t *songname3;
    lv_obj_t *songname4;
} objects_t;

extern objects_t objects;

void create_screen_list_page();
void tick_screen_list_page();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/