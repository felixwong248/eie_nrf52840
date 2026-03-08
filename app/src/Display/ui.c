#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>

#include <lvgl.h>

#include "ui.h"
#include "storage_init.h"

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_obj_t *screen = NULL;
static lv_obj_t *song_label = NULL;

int ui_init(void)
{
    if (!device_is_ready(display_dev)) {
        printk("Display device not ready\n");
        return -1;
    }

    screen = lv_screen_active();
    if(screen == NULL) {
        return -2;
    }
    song_label = lv_label_create(screen);
    lv_obj_align(song_label, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_label_set_text(song_label, "Loading...");

    display_blanking_off(display_dev);

    return 0;
}

int ui_show_first_song(void)
{
    char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
    int file_count;

    file_count = file_name_read(file_names);
    if (file_count < 0) {
        printk("file_name_read failed rc=%d\n", file_count);
        lv_label_set_text(song_label, "Read error");
        return file_count;
    }

    if (file_count == 0) {
        lv_label_set_text(song_label, "No songs found");
        return 0;
    }

    lv_label_set_text(song_label, file_names[0]);
    return 0;
}