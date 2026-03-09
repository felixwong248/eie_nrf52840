#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

#include "old_ui.h"
#include "storage_init.h"

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static lv_obj_t *screen = NULL;
static lv_obj_t *song_container = NULL;

int ui_init(void)
{
    if (!device_is_ready(display_dev)) {
        printk("Display device not ready\n");
        return -1;
    }

    screen = lv_obj_create(NULL);
    if (screen == NULL) {
        return -2;
    }

    lv_obj_set_size(screen, 320, 240);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_screen_load(screen);
    display_blanking_off(display_dev);

    return 0;
}

int ui_show_song_list(void)
{
    char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
    int file_count;

    file_count = file_name_read(file_names);
    if (file_count < 0) {
        printk("file_name_read failed rc=%d\n", file_count);
        return file_count;
    }

    if (file_count == 0) {
        printk("No songs found\n");
        return -1;
    }

    /* ---------- TITLE ---------- */

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Music Player");

    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 6);


    /* ---------- SONG LIST CONTAINER ---------- */

    song_container = lv_obj_create(screen);

    lv_obj_set_size(song_container, 320, 200);
    lv_obj_set_pos(song_container, 0, 30);   // leave space for title

    lv_obj_set_style_bg_opa(song_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(song_container, 0, 0);

    lv_obj_set_style_pad_left(song_container, 6, 0);
    lv_obj_set_style_pad_right(song_container, 6, 0);
    lv_obj_set_style_pad_top(song_container, 6, 0);

    lv_obj_set_flex_flow(song_container, LV_FLEX_FLOW_COLUMN);

    lv_obj_clear_flag(song_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(song_container, LV_OBJ_FLAG_CLICKABLE);


    /* ---------- SONG ROWS ---------- */

    for (int i = 0; i < file_count; i++) {

        lv_obj_t *row = lv_obj_create(song_container);
        lv_obj_set_width(row, 300);
        lv_obj_set_height(row, 30);

        lv_obj_set_style_bg_color(row, lv_color_hex(0xCCFFFF), 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 0, 0);

        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *label = lv_label_create(row);

        lv_label_set_text(label, file_names[i]);

        /* smaller font */
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);

        /* LEFT aligned instead of centered */
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
    }

    return 0;
}