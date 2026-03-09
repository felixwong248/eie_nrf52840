#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>

#include "storage_init.h"
#include "button_control.h"
#include "player_controls.h"
#include "ui.h"
#include "actions.h"

int main(void)
{
    int rc;
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display device not ready\n");
        return 0;
    }

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

    ui_init();
    display_blanking_off(display_dev);
    load_song_menu();

    player_control_start();

    while (1) {
        lv_timer_handler();
        ui_tick();
        menu_process_requests();
        k_sleep(K_MSEC(10));
        
    }

    return 0;
}