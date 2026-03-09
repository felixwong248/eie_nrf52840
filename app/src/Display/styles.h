#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: Song name text bbl
lv_style_t *get_style_song_name_text_bbl_MAIN_DEFAULT();
void add_style_song_name_text_bbl(lv_obj_t *obj);
void remove_style_song_name_text_bbl(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/