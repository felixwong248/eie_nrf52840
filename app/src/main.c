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

    while(1) {
 
        k_msleep(SLEEP_MS);
    }
}