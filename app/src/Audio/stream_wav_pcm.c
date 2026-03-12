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
#include "wav_parser.h"
#include "global_variables.h"


#define I2S_TIMEOUT_MS   2000

// I2s needs fixed sized memory blocks (all considered a slab) when that are dedicated to i2s 
// better than malloc because easier and faster to allocate memory which is needed for audio.
// You want multiple memory blocks so that you can stream and process data at the same time
#define I2S_BLOCK_SIZE   2048 // fill chunks of 2048 bytes
#define I2S_BLOCK_COUNT  8    // 8 memory blocks for buffering for i2s
K_MEM_SLAB_DEFINE(i2s_slab, I2S_BLOCK_SIZE, I2S_BLOCK_COUNT, 4);

static bool i2s_is_configured = false;
static uint32_t configured_sample_rate = 0;

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));

static int i2s_config_tx(uint32_t sample_rate_hz)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S device not ready\n");
        return -ENODEV;
    }

    struct i2s_config cfg = {
        .word_size      = 16, // 16 bit stereo audio
        .channels       = 2,  // 2 channels, L and R
        .format         = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED, // board kept outputting in left Jus even in standard philips i2s
        .options        = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER, // board generates bit clk for dac
        .frame_clk_freq = sample_rate_hz, // play back speed matches the sample rate of the wav file
        .block_size     = I2S_BLOCK_SIZE,
        .mem_slab       = &i2s_slab,
        .timeout        = I2S_TIMEOUT_MS
    };

    int rc = i2s_configure(i2s_dev, I2S_DIR_TX, &cfg);
    printk("i2s_configure rc=%d\n", rc);
    
    return rc;
}

static inline void reclaim_mem_block(void)
{
    void *released = NULL;
    size_t released_size = 0;

    // i2s_read checks if any blocks are free, and if yes, free the block
    int rc = i2s_read(i2s_dev, &released, &released_size);
    if (rc == 0 && released != NULL) {
        k_mem_slab_free(&i2s_slab, released);
    }
}

static void i2s_stop_tx(bool immediate_stop)
{
    int rc;

    if (immediate_stop) {
        rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
        printk("i2s_stop_tx(): DROP rc=%d\n", rc);
    } else {
        rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
        printk("i2s_stop_tx(): DRAIN rc=%d\n", rc);

        if (rc != 0) {
            rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
            printk("i2s_stop_tx(): DRAIN failed, DROP rc=%d\n", rc);
        }
    }
}

int stream_pcm(const char *path, const struct wav_info *info)
{
    int rc;

    if (!i2s_is_configured || configured_sample_rate != info->sample_rate) {
        rc = i2s_config_tx(info->sample_rate);
        if (rc) {
            printk("stream_pcm(): i2s_config_tx failed rc=%d\n", rc);
            return rc;
        }

        i2s_is_configured = true;
        configured_sample_rate = info->sample_rate;
    }

    struct fs_file_t f;
    fs_file_t_init(&f);

    rc = fs_open(&f, path, FS_O_READ);
    printk("stream_pcm(): fs_open rc=%d\n", rc);
    if (rc < 0){
        return rc;
    }

    rc = fs_seek(&f, info->data_offset, FS_SEEK_SET); // skips the formatting chunks and goes to data chunks
    printk("stream_pcm(): fs_seek rc=%d\n", rc);
    if (rc < 0) {
        fs_close(&f);
        return rc;
    }

    size_t bytes_left = info->data_size;
    for (int n = 0; n < (I2S_BLOCK_COUNT / 2) && bytes_left > 0; n++) { // prime memory blocks with data to prevent delays
        void *block = NULL;

        rc = k_mem_slab_alloc(&i2s_slab, &block, K_FOREVER); // allocates a free mem block
        if (rc) {
            printk("stream_pcm(): slab alloc prime rc=%d\n", rc);
            rc = -ENOMEM;
            goto out;
        }

        // read size takes the smaller value of the block size or how ever many bytes are left
        // and then rounds it down so its a multiple of block align so you don't have half an audio frame
        size_t read_size = MIN(bytes_left, (size_t)I2S_BLOCK_SIZE);
        read_size -= (read_size % info->block_align);
        if (read_size == 0) {
            k_mem_slab_free(&i2s_slab, block);
            break;
        }

        ssize_t r = fs_read(&f, block, read_size);
        if (r <= 0) {
            printk("stream_pcm(): fs_read(prime mem blocks) r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, block);
            rc = -EIO;
            goto out;
        }

        // fills the rest of the block with 0s if theres less bytes read than the 2048 block size
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
    if (rc) {
        goto out;
    }
    printk("stream_pcm(): streaming loop...\n");

    uint32_t blocks_sent = 0;

    while (bytes_left > 0) {

        if (g_next_requested || g_prev_requested || g_stop_requested) {
        printk("stream_pcm(): stop/skip requested\n");
        rc = 0;
        break;
        }
        void *block = NULL;


        reclaim_mem_block();

        rc = k_mem_slab_alloc(&i2s_slab, &block, K_FOREVER);
        if (rc) {
            printk("stream_pcm(): slab alloc loop rc=%d\n", rc);
            rc = -ENOMEM;
            break;
        }

        size_t read_size = MIN(bytes_left, (size_t)I2S_BLOCK_SIZE);
        read_size -= (read_size % info->block_align);
        if (read_size == 0) {
            k_mem_slab_free(&i2s_slab, block);
            break;
        }

        ssize_t r = fs_read(&f, block, read_size);
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

        blocks_sent++;
        if ((blocks_sent % 1000) == 0) {
            printk("stream_pcm(): blocks_sent=%u bytes_left=%u\n", blocks_sent, bytes_left);
        }
    }

out:
    printk("stream_pcm(): stopping rc=%d\n", rc);
    fs_close(&f);
    i2s_stop_tx(rc!=0);
    printk("stream_pcm(): exit rc=%d\n", rc);

    if (rc != 0) {
    i2s_is_configured = false;
    configured_sample_rate = 0;
}
    return rc;
}

int play_current_file(const char *path, struct wav_info *info)
{
    int rc;

    rc = parse_wav(path, info);
    printk("parse_wav rc=%d\n", rc);
    if (rc != 0) {
        return rc;
    }

    rc = stream_pcm(path, info);
    printk("stream_pcm rc=%d\n", rc);
    if (rc != 0) {
        return rc;
    }

    return 0;
}