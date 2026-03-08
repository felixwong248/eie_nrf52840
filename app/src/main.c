#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#include "BTN.h"

#include "storage_init.h"
#include "wav_parser.h"
#include "stream_wav_pcm.h"

#define WAV_PATH "/SD:/week.wav"
#define SLEEP_MS 1

int main(void)
{

    int rc;
    struct wav_info info;
    
    int file_count = 0;
    int current_file_index = 0;
    char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
    char current_path[128];

    printk("SD FATFS WAV test start\n");

    rc = storage_init();
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

    build_wav_path(current_path, sizeof(current_path), file_names[current_file_index]);
    printk("Current path: %s\n", current_path);

    for (int i = 0; i < file_count; i++) {
        printk("%d: %s\n", i, file_names[i]);
    }

    printk("------------------------\n");

    printk("Done listing files.\n");
   
    rc = BTN_init_selected(BTN2);
    printk("BTN_init_selected(BTN2) rc=%d\n", rc);
    if (rc != 0) {
        printk("BTN2 init failed rc=%d\n", rc);
        return 0;
    }

    rc = BTN_init_selected(BTN3);
    printk("BTN_init_selected(BTN3) rc=%d\n", rc);
    if (rc != 0) {
        printk("BTN3 init failed rc=%d\n", rc);
        return 0;
    }

    while (1)
    {
        if (BTN_check_clear_pressed(BTN2))
        {
            printk("Button pressed\n");

            current_file_index++;
            if (current_file_index >= file_count) {
                current_file_index = 0;
            }

            build_wav_path(current_path, sizeof(current_path),
                        file_names[current_file_index]);

            printk("New file: %s\n", current_path);

            rc = play_current_file(current_path, &info);
            printk("play_current_file rc=%d\n", rc);
        }

        k_sleep(K_MSEC(10));
    }

}