#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "stream_wav_pcm.h"

#define I2S_BLOCK_SIZE   2048
#define I2S_BLOCK_COUNT  8
#define I2S_TIMEOUT_MS   2000

K_MEM_SLAB_DEFINE(i2s_slab, I2S_BLOCK_SIZE, I2S_BLOCK_COUNT, 4);

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));

static int i2s_config_tx(uint32_t sample_rate_hz)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S device not ready\n");
        return -ENODEV;
    }

    struct i2s_config cfg = {0};
    cfg.word_size      = 16;
    cfg.channels       = 2;

    /* Match your WORKING demo exactly */
    cfg.format         = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED;

    cfg.options        = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
    cfg.frame_clk_freq = sample_rate_hz;
    cfg.block_size     = I2S_BLOCK_SIZE;
    cfg.mem_slab       = &i2s_slab;
    cfg.timeout        = I2S_TIMEOUT_MS;

    int rc = i2s_configure(i2s_dev, I2S_DIR_TX, &cfg);
    printk("i2s_configure rc=%d\n", rc);
    return rc;
}

static inline void reclaim_one(void)
{
    void *released = NULL;
    size_t released_size = 0;

    int rc = i2s_read(i2s_dev, &released, &released_size);
    if (rc == 0 && released != NULL) {
        k_mem_slab_free(&i2s_slab, released);
    }
}

static void i2s_stop_tx(void)
{
    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);

    /* best-effort reclaim */
    for (int i = 0; i < I2S_BLOCK_COUNT; i++) {
        reclaim_one();
    }
}

int stream_pcm(const char *path, const struct wav_info *info)
{
    printk("stream_pcm(): enter\n");

    printk("stream_pcm(): WAV fmt=%u ch=%u fs=%u bits=%u align=%u off=%lld size=%u\n",
           info->audio_format, info->num_channels, info->sample_rate,
           info->bits_per_sample, info->block_align,
           (long long)info->data_offset, info->data_size);

    if (info->audio_format != 1 || info->bits_per_sample != 16 || info->num_channels != 2) {
        printk("stream_pcm(): unsupported WAV\n");
        return -ENOTSUP;
    }

    int rc = i2s_config_tx(info->sample_rate);
    if (rc) {
        printk("stream_pcm(): i2s_config_tx failed rc=%d\n", rc);
        return rc;
    }

    struct fs_file_t f;
    fs_file_t_init(&f);

    printk("stream_pcm(): fs_open(%s)\n", path);
    rc = fs_open(&f, path, FS_O_READ);
    printk("stream_pcm(): fs_open rc=%d\n", rc);
    if (rc < 0) return rc;

    printk("stream_pcm(): fs_seek data_offset=%lld\n", (long long)info->data_offset);
    rc = fs_seek(&f, info->data_offset, FS_SEEK_SET);
    printk("stream_pcm(): fs_seek rc=%d\n", rc);
    if (rc < 0) {
        fs_close(&f);
        return rc;
    }

    size_t bytes_left = info->data_size;

    /* PRIME QUEUE BEFORE START (like your working demo) */
    printk("stream_pcm(): priming...\n");
    for (int n = 0; n < (I2S_BLOCK_COUNT / 2) && bytes_left > 0; n++) {
        void *block = NULL;

        rc = k_mem_slab_alloc(&i2s_slab, &block, K_FOREVER);
        if (rc) {
            printk("stream_pcm(): slab alloc prime rc=%d\n", rc);
            rc = -ENOMEM;
            goto out;
        }

        size_t want = MIN(bytes_left, (size_t)I2S_BLOCK_SIZE);
        want -= (want % info->block_align);
        if (want == 0) {
            k_mem_slab_free(&i2s_slab, block);
            break;
        }

        ssize_t r = fs_read(&f, block, want);
        if (r <= 0) {
            printk("stream_pcm(): fs_read(prime) r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, block);
            rc = -EIO;
            goto out;
        }

        if ((size_t)r < I2S_BLOCK_SIZE) {
            memset((uint8_t *)block + (size_t)r, 0, I2S_BLOCK_SIZE - (size_t)r);
        }

        bytes_left -= (size_t)r;

        rc = i2s_write(i2s_dev, block, I2S_BLOCK_SIZE);
        printk("stream_pcm(): prime i2s_write[%d] rc=%d\n", n, rc);
        if (rc) {
            k_mem_slab_free(&i2s_slab, block);
            goto out;
        }
    }

    printk("stream_pcm(): START\n");
    rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
    printk("stream_pcm(): i2s_trigger(START) rc=%d\n", rc);
    if (rc) goto out;

    printk("stream_pcm(): streaming loop...\n");

    uint32_t blocks = 0;

    while (bytes_left > 0) {
        void *block = NULL;

        /* keep slab healthy */
        reclaim_one();

        rc = k_mem_slab_alloc(&i2s_slab, &block, K_FOREVER);
        if (rc) {
            printk("stream_pcm(): slab alloc loop rc=%d\n", rc);
            rc = -ENOMEM;
            break;
        }

        size_t want = MIN(bytes_left, (size_t)I2S_BLOCK_SIZE);
        want -= (want % info->block_align);
        if (want == 0) {
            k_mem_slab_free(&i2s_slab, block);
            break;
        }

        ssize_t r = fs_read(&f, block, want);
        if (r <= 0) {
            printk("stream_pcm(): fs_read(loop) r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, block);
            rc = -EIO;
            break;
        }

        if ((size_t)r < I2S_BLOCK_SIZE) {
            memset((uint8_t *)block + (size_t)r, 0, I2S_BLOCK_SIZE - (size_t)r);
        }

        bytes_left -= (size_t)r;

        rc = i2s_write(i2s_dev, block, I2S_BLOCK_SIZE);
        if (rc) {
            printk("stream_pcm(): i2s_write(loop) rc=%d\n", rc);
            k_mem_slab_free(&i2s_slab, block);
            break;
        }

        blocks++;
        if ((blocks % 50) == 0) {
            printk("stream_pcm(): blocks=%u bytes_left=%u\n",
                   (unsigned)blocks, (unsigned)bytes_left);
        }
    }

out:
    printk("stream_pcm(): stopping rc=%d\n", rc);
    fs_close(&f);
    i2s_stop_tx();
    printk("stream_pcm(): exit rc=%d\n", rc);
    return rc;
}