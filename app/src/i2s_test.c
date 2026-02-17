/* i2s_tone_test.c
 * Simple I2S tone test (16-bit stereo, 48 kHz) for PCM5102A.
 * Plays a ~440 Hz sine-ish tone for a few seconds.
 *
 * Assumes:
 *  - i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0)) exists and is ready
 *  - i2s_slab is defined (I2S_BLOCK_SIZE / COUNT)
 *  - nRF is I2S master (BCLK+LRCLK out), PCM5102A is sink
 */

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

/* Use your existing defines if already present */
#ifndef I2S_BLOCK_SIZE
#define I2S_BLOCK_SIZE   2048
#endif
#ifndef I2S_BLOCK_COUNT
#define I2S_BLOCK_COUNT  8
#endif
#ifndef I2S_TIMEOUT_MS
#define I2S_TIMEOUT_MS   2000
#endif

K_MEM_SLAB_DEFINE(i2s_slab, I2S_BLOCK_SIZE, I2S_BLOCK_COUNT, 4);

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));

static int i2s_config_tx_stereo_16(uint32_t sample_rate_hz)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S device not ready\n");
        return -ENODEV;
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
    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
    (void)i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);
}

/* Tiny sine-ish lookup table (one period), Q15-ish amplitude.
 * This keeps the code simple (no math library).
 * 64 samples per period -> tone freq = Fs/64.
 * At 48kHz, Fs/64 = 750 Hz. (Not 440, but clearly audible.)
 */
static const int16_t sin64[64] = {
      0,  3212,  6393,  9512, 12539, 15446, 18204, 20787,
  23170, 25329, 27245, 28898, 30273, 31356, 32137, 32610,
  32767, 32610, 32137, 31356, 30273, 28898, 27245, 25329,
  23170, 20787, 18204, 15446, 12539,  9512,  6393,  3212,
      0, -3212, -6393, -9512,-12539,-15446,-18204,-20787,
 -23170,-25329,-27245,-28898,-30273,-31356,-32137,-32610,
 -32767,-32610,-32137,-31356,-30273,-28898,-27245,-25329,
 -23170,-20787,-18204,-15446,-12539, -9512, -6393, -3212
};

static void fill_stereo_block_16(void *block, uint32_t frames, uint32_t *phase, int16_t amp)
{
    /* block is interleaved L,R 16-bit */
    int16_t *p = (int16_t *)block;

    for (uint32_t i = 0; i < frames; i++) {
        int16_t s = (int16_t)((amp * (int32_t)sin64[*phase]) / 32767);

        /* L then R */
        *p++ = s;
        *p++ = s;

        *phase = (*phase + 1) & 63; /* 0..63 */
    }
}

int i2s_tone_test(void)
{
    const uint32_t fs = 48000;               /* common, easy clocking */
    const uint32_t bytes_per_frame = 2 /*ch*/ * 2 /*bytes*/;  /* 4 */
    const uint32_t frames_per_block = I2S_BLOCK_SIZE / bytes_per_frame;

    int rc = i2s_config_tx_stereo_16(fs);
    if (rc) return rc;

    /* Prime 2 blocks */
    uint32_t phase = 0;
    int16_t amp = 9000; /* keep it moderate to avoid clipping */

    for (int k = 0; k < 2; k++) {
        void *blk = NULL;
        rc = k_mem_slab_alloc(&i2s_slab, &blk, K_MSEC(I2S_TIMEOUT_MS));
        if (rc) {
            printk("slab alloc prime failed: %d\n", rc);
            rc = -ENOMEM;
            goto out;
        }

        fill_stereo_block_16(blk, frames_per_block, &phase, amp);

        rc = i2s_write(i2s_dev, blk, I2S_BLOCK_SIZE);
        if (rc) {
            printk("i2s_write prime failed: %d\n", rc);
            k_mem_slab_free(&i2s_slab, &blk);
            goto out;
        }
    }

    rc = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
    if (rc) {
        printk("i2s_trigger START failed: %d\n", rc);
        goto out;
    }

    /* Push enough blocks for ~3 seconds */
    const uint32_t blocks_per_sec = fs / frames_per_block;
    const uint32_t total_blocks = blocks_per_sec * 3;

    for (uint32_t n = 0; n < total_blocks; n++) {
        void *blk = NULL;
        rc = k_mem_slab_alloc(&i2s_slab, &blk, K_MSEC(I2S_TIMEOUT_MS));
        if (rc) {
            printk("slab alloc failed: %d\n", rc);
            rc = -ENOMEM;
            break;
        }

        fill_stereo_block_16(blk, frames_per_block, &phase, amp);

        rc = i2s_write(i2s_dev, blk, I2S_BLOCK_SIZE);
        if (rc) {
            printk("i2s_write failed: %d\n", rc);
            k_mem_slab_free(&i2s_slab, &blk);
            break;
        }
    }

out:
    i2s_stop_tx();
    return rc;
}


