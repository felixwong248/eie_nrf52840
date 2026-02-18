#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>

#include "lvgl.h"

#include "BTN.h"
//#include "lv_data_obj.h"

#include "storage_init.h"
#include "wav_parser.h"
#include "stream_wav_pcm.h"
//#include "i2s_test.h"

#define WAV_PATH "/SD:/test2.wav"
#define SLEEP_MS 1

static const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static lv_obj_t *screen = NULL;

int main(void)
{
    if(!device_is_ready(display_dev)){
        return 0;
    }
    screen = lv_screen_active();
    if(screen == NULL){
        return 0;
    }

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Hello World!");
    display_blanking_off(display_dev);
    /*
    int rc;
    struct wav_info info;
    
    int file_count;
    char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
    
    printk("SD FATFS WAV test start\n");

    rc = storage_init();   // now mounts SD, not RAM
    if (rc != 0) {
        printk("storage_init failed rc=%d\n", rc);
        return 0;
    }

    file_count = file_name_read(file_names);

    if (file_count < 0) {
        printk("file_name_read failed\n");
        return 0;
    }

    printk("\nFiles found on SD card:\n");
    printk("------------------------\n");

    for (int i = 0; i < file_count; i++) {
        printk("%d: %s\n", i, file_names[i]);
    }

    printk("------------------------\n");

    printk("Done listing files.\n");

    rc = parse_wav(WAV_PATH, &info);
    printk("parse_wav rc=%d\n", rc);

    rc = stream_pcm(WAV_PATH, &info);
    printk("stream_pcm rc=%d\n", rc);
    
    printk("Done.\n");
    */
    while(1) {
        lv_timer_handler();
        k_msleep(SLEEP_MS);
    }
}
