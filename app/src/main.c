#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "storage_init.h"
#include "button_control.h"
#include "player_controls.h"

int main(void)
{
    int rc;

    printk("SD FATFS WAV test start\n");

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

    player_control_run();

    return 0;
}