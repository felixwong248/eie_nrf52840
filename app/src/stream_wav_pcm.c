#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "wav_parser.h"

/* Must be multiple of frame_size where frame_size = channels * (word_size_bytes)
 * For 16-bit stereo: frame_size = 2 * 2 = 4 bytes, 2048 is OK.
 */
#define I2S_BLOCK_SIZE   2048
#define I2S_BLOCK_COUNT  8
#define I2S_TIMEOUT_MS   2000

K_MEM_SLAB_DEFINE(i2s_slab, I2S_BLOCK_SIZE, I2S_BLOCK_COUNT, 4);

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));

static int i2s_config_tx(uint32_t sample_rate_hz, uint8_t channels, uint8_t bits_per_sample)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S device not ready\n");
        return -ENODEV;
    }

    if (bits_per_sample != 16) {
        printk("Only 16-bit PCM supported\n");
        return -ENOTSUP;
    }

    if (channels != 2) {
        printk("Only stereo supported (channels=%u)\n", channels);
        return -ENOTSUP;
    }

    struct i2s_config cfg = {0};

    cfg.word_size      = 16;
    cfg.channels       = 2;

    cfg.format         = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_DATA_ORDER_MSB;

    cfg.options        = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
    cfg.frame_clk_freq = sample_rate_hz;
    cfg.block_size     = I2S_BLOCK_SIZE;
    cfg.mem_slab       = &i2s_slab;
    cfg.timeout        = I2S_TIMEOUT_MS;

    int rc = i2s_configure(i2s_dev, I2S_DIR_TX, &cfg);
    if (rc) {
        printk("i2s_configure(TX) failed: %d\n", rc);
    }
    return rc;
}

static void i2s_stop_tx(void)
{
    /* DRAIN sends everything queued then returns to READY */
    int rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
    if (rc) {
        /* If we're not RUNNING, DRAIN can fail with -EIO. That's OK; just STOP. */
        (void)rc;
    }

    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);
}


int stream_pcm(const char *path, const struct wav_info *info)
{
    int rc;

    /* ---- WAV sanity: ONLY 16-bit PCM STEREO ---- */
    if (info->audio_format != 1) {
        printk("Not PCM (audio_format=%u)\n", info->audio_format);
        return -ENOTSUP;
    }
    if (info->bits_per_sample != 16) {
        printk("Only 16-bit supported (bits=%u)\n", info->bits_per_sample);
        return -ENOTSUP;
    }
    if (info->num_channels != 2) {
        printk("Only stereo supported (channels=%u)\n", info->num_channels);
        return -ENOTSUP;
    }

    /* ---- Configure I2S TX ---- */
    rc = i2s_config_tx(info->sample_rate, 2, 16);
    if (rc) {
        return rc;
    }

    /* ---- Open and seek WAV data ---- */
    struct fs_file_t f;
    fs_file_t_init(&f);

    rc = fs_open(&f, path, FS_O_READ);
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

    size_t bytes_left = info->data_size;

    /* temp buffer for reading from file */
    uint8_t in_buf[I2S_BLOCK_SIZE];

    /* ---- PRIME: queue 2 TX blocks BEFORE START ---- */
    for (int primed = 0; primed < 2 && bytes_left > 0; primed++) {

        void *tx_block = NULL;
        rc = k_mem_slab_alloc(&i2s_slab, &tx_block, K_MSEC(I2S_TIMEOUT_MS));
        if (rc) {
            printk("k_mem_slab_alloc(prime) failed: %d\n", rc);
            rc = -ENOMEM;
            goto out;
        }

        size_t want = MIN(bytes_left, (size_t)I2S_BLOCK_SIZE);
        want -= (want % info->block_align);
        if (want == 0) {
            k_mem_slab_free(&i2s_slab, &tx_block);
            break;
        }

        ssize_t r = fs_read(&f, in_buf, want);
        if (r <= 0) {
            printk("fs_read(prime) failed r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, &tx_block);
            rc = -EIO;
            goto out;
        }

        memcpy(tx_block, in_buf, (size_t)r);
        if ((size_t)r < I2S_BLOCK_SIZE) {
            memset((uint8_t *)tx_block + (size_t)r, 0, I2S_BLOCK_SIZE - (size_t)r);
        }
        bytes_left -= (size_t)r;

        rc = i2s_write(i2s_dev, tx_block, I2S_BLOCK_SIZE);
        if (rc) {
            printk("i2s_write(prime) failed: %d\n", rc);
            k_mem_slab_free(&i2s_slab, &tx_block);
            goto out;
        }
    }

    /* ---- START ---- */
    rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
    if (rc) {
        printk("i2s_trigger(START) failed: %d\n", rc);
        goto out;
    }

    /* ---- Stream loop ---- */
    while (bytes_left > 0) {

        void *tx_block = NULL;
        rc = k_mem_slab_alloc(&i2s_slab, &tx_block, K_MSEC(I2S_TIMEOUT_MS));
        if (rc) {
            printk("k_mem_slab_alloc failed: %d\n", rc);
            rc = -ENOMEM;
            break;
        }

        size_t want = MIN(bytes_left, (size_t)I2S_BLOCK_SIZE);
        want -= (want % info->block_align);
        if (want == 0) {
            k_mem_slab_free(&i2s_slab, &tx_block);
            break;
        }

        ssize_t r = fs_read(&f, in_buf, want);
        if (r <= 0) {
            printk("fs_read failed r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, &tx_block);
            rc = -EIO;
            break;
        }

        memcpy(tx_block, in_buf, (size_t)r);
        if ((size_t)r < I2S_BLOCK_SIZE) {
            memset((uint8_t *)tx_block + (size_t)r, 0, I2S_BLOCK_SIZE - (size_t)r);
        }
        bytes_left -= (size_t)r;

        rc = i2s_write(i2s_dev, tx_block, I2S_BLOCK_SIZE);
        if (rc) {
            printk("i2s_write failed: %d\n", rc);
            k_mem_slab_free(&i2s_slab, &tx_block);
            break;
        }
    }

out:
    fs_close(&f);
    i2s_stop_tx();
    return rc;
}
