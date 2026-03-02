#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/fs/fs.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "wav_parser.h"

/* We will transmit 32-bit slots (stereo => 8 bytes per frame).
 * 2048 bytes is divisible by 8, so this is fine.
 */
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

    cfg.word_size      = 16;                 /* start simple: 16-bit */
    cfg.channels       = 2;
    cfg.format         = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_CLK_NF_NB;
    cfg.options        = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
    cfg.frame_clk_freq = sample_rate_hz;

    /* Must be multiple of frame_size = channels * (word_size/8) = 2*2=4 */
    cfg.block_size     = I2S_BLOCK_SIZE;
    cfg.mem_slab       = &i2s_slab;

    /* nRF I2S driver is picky; safest is 0 */
    cfg.timeout        = 0;

    int rc = i2s_configure(i2s_dev, I2S_DIR_TX, &cfg);
    if (rc) {
        printk("i2s_configure(TX) failed: %d\n", rc);
    }
    return rc;
}

static void i2s_stop_tx(void)
{
    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);
}

/* Convert little-endian 16-bit stereo PCM -> 32-bit slot stereo (MSB aligned).
 * Input frame:  [L16][R16]  (4 bytes)
 * Output frame: [L32][R32]  (8 bytes) where L32 = L16<<16, R32 = R16<<16
 */
static size_t convert_16_to_32_slots(uint8_t *out_block, size_t out_cap,
                                    const uint8_t *in, size_t in_len)
{
    /* out_cap is I2S_BLOCK_SIZE, in_len is multiple of 4 */
    size_t out_pos = 0;
    size_t in_pos = 0;

    while ((in_pos + 4) <= in_len && (out_pos + 8) <= out_cap) {
        int16_t l16 = (int16_t)((uint16_t)in[in_pos] | ((uint16_t)in[in_pos + 1] << 8));
        int16_t r16 = (int16_t)((uint16_t)in[in_pos + 2] | ((uint16_t)in[in_pos + 3] << 8));
        in_pos += 4;

        int32_t l32 = ((int32_t)l16) << 16;
        int32_t r32 = ((int32_t)r16) << 16;

        /* little-endian write */
        out_block[out_pos + 0] = (uint8_t)(l32 & 0xFF);
        out_block[out_pos + 1] = (uint8_t)((l32 >> 8) & 0xFF);
        out_block[out_pos + 2] = (uint8_t)((l32 >> 16) & 0xFF);
        out_block[out_pos + 3] = (uint8_t)((l32 >> 24) & 0xFF);

        out_block[out_pos + 4] = (uint8_t)(r32 & 0xFF);
        out_block[out_pos + 5] = (uint8_t)((r32 >> 8) & 0xFF);
        out_block[out_pos + 6] = (uint8_t)((r32 >> 16) & 0xFF);
        out_block[out_pos + 7] = (uint8_t)((r32 >> 24) & 0xFF);

        out_pos += 8;
    }

    return out_pos; /* bytes produced */
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

    /* ---- Configure I2S TX for 32-bit slots ---- */
    rc = i2s_config_tx(info->sample_rate);
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

    /* Read buffer: we will read up to half a TX block worth of 16-bit stereo,
     * because conversion doubles the bytes (4->8 per frame).
     */
    uint8_t in_buf[I2S_BLOCK_SIZE / 2];

    /* ---- PRIME: queue 2 TX blocks BEFORE START ---- */
    for (int primed = 0; primed < 2 && bytes_left > 0; primed++) {

        void *tx_block = NULL;
        rc = k_mem_slab_alloc(&i2s_slab, &tx_block, K_MSEC(I2S_TIMEOUT_MS));
        if (rc) {
            printk("k_mem_slab_alloc(prime) failed: %d\n", rc);
            rc = -ENOMEM;
            goto out;
        }

        /* Read size must be multiple of WAV block_align (likely 4 bytes) */
        size_t want_in = MIN(bytes_left, (size_t)sizeof(in_buf));
        want_in -= (want_in % info->block_align);
        if (want_in == 0) {
            k_mem_slab_free(&i2s_slab, &tx_block);
            break;
        }

        ssize_t r = fs_read(&f, in_buf, want_in);
        if (r <= 0) {
            printk("fs_read(prime) failed r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, &tx_block);
            rc = -EIO;
            goto out;
        }

        /* Convert into TX block (32-bit slots) */
        memset(tx_block, 0, I2S_BLOCK_SIZE);
        size_t produced = convert_16_to_32_slots((uint8_t *)tx_block, I2S_BLOCK_SIZE,
                                                in_buf, (size_t)r);

        /* Keep bytes_left accounting based on input bytes consumed */
        bytes_left -= (size_t)r;

        /* Always queue full block_size to keep I2S happy */
        rc = i2s_write(i2s_dev, tx_block, I2S_BLOCK_SIZE);
        if (rc) {
            printk("i2s_write(prime) failed: %d\n", rc);
            k_mem_slab_free(&i2s_slab, &tx_block);
            goto out;
        }

        (void)produced; /* produced is for debugging if you want to printk it */
    }

    /* Some drivers behave better with PREPARE first */
    rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_PREPARE);
    if (rc) {
        printk("i2s_trigger(PREPARE) failed: %d\n", rc);
        goto out;
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

        size_t want_in = MIN(bytes_left, (size_t)sizeof(in_buf));
        want_in -= (want_in % info->block_align);
        if (want_in == 0) {
            k_mem_slab_free(&i2s_slab, &tx_block);
            break;
        }

        ssize_t r = fs_read(&f, in_buf, want_in);
        if (r <= 0) {
            printk("fs_read failed r=%d\n", (int)r);
            k_mem_slab_free(&i2s_slab, &tx_block);
            rc = -EIO;
            break;
        }

        memset(tx_block, 0, I2S_BLOCK_SIZE);
        (void)convert_16_to_32_slots((uint8_t *)tx_block, I2S_BLOCK_SIZE,
                                     in_buf, (size_t)r);

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