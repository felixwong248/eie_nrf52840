#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

#include "storage_init.h"
#include "button_control.h"
#include "player_controls.h"
#include "old_ui.h"

int main(void)
{
    int rc;

    rc = storage_init();
    if (rc != 0) {
        printk("storage_init failed rc=%d\n", rc);
        return 0;
    }
    
    rc = player_control_init();
    if (rc != 0) {
        printk("player_init failed rc=%d\n", rc);
        return 0;
    }

    rc = button_control_init();
    if (rc != 0) {
        printk("button_control_init failed rc=%d\n", rc);
        return 0;
    }
    /*
    rc = ui_init();
    if (rc != 0) {
        printk("ui_init failed rc=%d\n", rc);
        return 0;
    }

    rc = ui_show_song_list();
    if (rc != 0) {
        printk("ui_show_first_song failed rc=%d\n", rc);
    }
    */
    player_control_start();

    while (1) {
        lv_timer_handler();
        k_sleep(K_MSEC(1));
    }

    return 0;
}