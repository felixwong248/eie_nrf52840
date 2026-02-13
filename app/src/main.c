#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "storage_init.h"
#include "wav_parser.h"
#include "stream_wav_pcm.h"

#define WAV_PATH "/SD:/test.wav"

int main(void)
{
    int rc;
    struct wav_info info;

    printk("SD FATFS WAV test start\n");

    rc = storage_init();   // now mounts SD, not RAM
    if (rc != 0) {
        printk("storage_init failed rc=%d\n", rc);
        return 0;
    }

    rc = parse_wav(WAV_PATH, &info);
    printk("parse_wav rc=%d\n", rc);

    printk("parse_wav OK\n");

    rc = stream_pcm(WAV_PATH, &info);
    printk("stream_pcm rc=%d\n", rc);

    printk("Done.\n");
    return 0;
}
