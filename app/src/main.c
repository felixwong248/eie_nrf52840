#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdint.h>

#include "test_wav.h"
#include "storage_init.h"
#include "wav_parser.h"

#define WAV_PATH "/RAM:/test.wav"


static int write_file_from_bytes(const char *path, const uint8_t *data, size_t len)
{
    struct fs_file_t f;
    fs_file_t_init(&f);

    // Opens wav file to be read
    int rc = fs_open(&f, path, FS_O_CREATE | FS_O_WRITE);
    if (rc < 0) {
        printk("fs_open(write) rc=%d\n", rc);
        return rc;
    }


    ssize_t w = fs_write(&f, data, len);
    if (w < 0) {
        printk("fs_write rc=%d\n", (int)w);
        fs_close(&f);
        return (int)w;
    }
    if ((size_t)w != len) {
        printk("fs_write short: wrote %d of %u\n", (int)w, (unsigned)len);
        fs_close(&f);
        return -EIO;
    }

    fs_close(&f);
    return 0;
}


int main(void)
{
    int rc;
    struct wav_info info;
    printk("RAM FATFS WAV test start\n");

    // Mounts storage
    rc = storage_init();
    if (rc != 0) {
        printk("storage_init failed rc=%d\n", rc);
        return 0;
    }

    // Write wav data into file to simulate sd card file
    rc = write_file_from_bytes(WAV_PATH, ucDataBlock, sizeof(ucDataBlock));
    printk("write wav rc=%d (len=%u)\n", rc, (unsigned)sizeof(ucDataBlock));
    if (rc != 0) return 0;

    // Parse simulated wav file
    rc = parse_wav(WAV_PATH, &info);
    printk("parse_wav rc=%d\n", rc);

    return 0;
}
