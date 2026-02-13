#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "wav_parser.h"

#define PCM_BUF_SIZE 512

int stream_pcm(const char *path, const struct wav_info *info)
{
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
    size_t bytes_left = info->data_size;

    /* WAV PCM is little-endian.
     * We'll print ONLY a few frames per block so serial doesn't explode.
     */
    const int max_frames_to_print = 8;
    uint32_t frame_counter = 0;

    while (bytes_left > 0) {
        size_t bytes_to_read = MIN(bytes_left, (size_t)PCM_BUF_SIZE);

        /* ensure whole frames */
        bytes_to_read -= (bytes_to_read % info->block_align);
        if (bytes_to_read == 0) {
            break;
        }

        ssize_t r = fs_read(&f, buffer, bytes_to_read);
        if (r <= 0) {
            printk("fs_read failed r=%d\n", (int)r);
            rc = -EIO;
            break;
        }

        /* ---- OUTPUT GOES HERE (print frames) ---- */
        int frames = (int)r / (int)info->block_align;
        int frames_to_print = MIN(frames, max_frames_to_print);

        /* Support common case: PCM 16-bit */
        if (info->bits_per_sample == 16) {
            if (info->num_channels == 1) {
                /* mono: each frame is one int16 sample */
                for (int i = 0; i < frames_to_print; i++) {
                    int idx = i * 2; /* 2 bytes per sample */
                    int16_t s = (int16_t)u16little_en(&buffer[idx]);
                    printk("frame %lu: mono=%d\n",
                           (unsigned long)(frame_counter + i), (int)s);
                }
            } else if (info->num_channels == 2) {
                /* stereo: each frame is L(int16), R(int16) */
                for (int i = 0; i < frames_to_print; i++) {
                    int base = i * info->block_align; /* usually 4 */
                    int16_t l = (int16_t)u16little_en(&buffer[base + 0]);
                    int16_t rr = (int16_t)u16little_en(&buffer[base + 2]);
                    printk("frame %lu: L=%d R=%d\n",
                           (unsigned long)(frame_counter + i), (int)l, (int)rr);
                }
            } else {
                printk("Unsupported channel count: %u\n", info->num_channels);
                rc = -ENOTSUP;
                break;
            }
        } else {
            printk("Unsupported bits_per_sample: %u\n", info->bits_per_sample);
            rc = -ENOTSUP;
            break;
        }

        printk("---- block: %d frames, %d bytes ----\n", frames, (int)r);

        frame_counter += (uint32_t)frames;
        bytes_left -= (size_t)r;

        /* Throttle so the serial monitor doesn't get nuked */
        //k_msleep(200);
    }

    fs_close(&f);
    return rc;
}
