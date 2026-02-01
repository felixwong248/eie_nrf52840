#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "wav_parser.h"

#define PCM_BUF_SIZE 512


int stream_pcm(const char *path, struct wav_info *info){
    struct fs_file_t f;
    fs_file_t_init(&f);


    int rc = fs_open(&f, path, FS_O_READ);
    if (rc < 0) {
        printk("fs_open() failed, rc=%d\n", rc);
        return rc;
    }

    rc = fs_seek(&f, info->data_offset, FS_SEEK_SET);
    if (rc < 0) {
        printk("fs_seek failed rc=%d\n", rc);
        fs_close(&f);
        return rc;
    }

    uint8_t buffer[PCM_BUF_SIZE];
    int frames_in_buffer = PCM_BUF_SIZE / info->block_align;

    size_t bytes_left = info->data_size;
    
    while (bytes_left > 0) {

        size_t bytes_to_read = MIN(bytes_left, PCM_BUF_SIZE);

        /* Make sure we read whole frames */
        bytes_to_read -= (bytes_to_read % info->block_align);

        ssize_t r = fs_read(&f, buffer, bytes_to_read);
        if (r <= 0) {
            printk("fs_read failed r=%d\n", (int)r);
            break;
        }

        /* Number of complete PCM frames in buffer */
        int frames = r / info->block_align;

        /* ---- THIS IS WHERE I2S WILL GO LATER ---- */
        /* For now, just pretend we consumed frames */

        bytes_left -= r;
}
}