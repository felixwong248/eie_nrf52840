#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

#define SAMPLE_RATE        48000
#define CHANNELS           2
#define WORD_SIZE_BITS     16

#define SAMPLES_PER_BLOCK  256
#define BLOCK_COUNT        8

#define BYTES_PER_SAMPLE   (WORD_SIZE_BITS / 8)
#define BLOCK_SIZE         (SAMPLES_PER_BLOCK * CHANNELS * BYTES_PER_SAMPLE)

#define I2S_TIMEOUT_MS     2000

K_MEM_SLAB_DEFINE(tx_slab, BLOCK_SIZE, BLOCK_COUNT, 4);

static void fill_block(int16_t *dst, uint32_t *phase)
{
    /* 1 kHz square at 48 kHz: toggle every 24 samples */
    const int16_t HI = 20000;
    const int16_t LO = -20000;

    for (int i = 0; i < SAMPLES_PER_BLOCK; i++) {
        int16_t v = ((*phase / 24U) & 1U) ? HI : LO;
        (*phase)++;

        dst[2 * i]     = v; /* L */
        dst[2 * i + 1] = v; /* R */
    }
}

int main(void)
{
    const struct device *i2s = DEVICE_DT_GET(DT_NODELABEL(i2s0));
    if (!device_is_ready(i2s)) {
        printk("I2S not ready\n");
        return 0;
    }

    printk("I2S dev: %s\n", i2s->name);

    struct i2s_config cfg = {0};
    cfg.word_size      = WORD_SIZE_BITS;
    cfg.channels       = CHANNELS;
    cfg.format = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_CLK_IF_NB;
    cfg.frame_clk_freq = SAMPLE_RATE;
    cfg.block_size     = BLOCK_SIZE;
    cfg.mem_slab       = &tx_slab;
    cfg.options        = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER;
    cfg.timeout        = I2S_TIMEOUT_MS;

    int ret = i2s_configure(i2s, I2S_DIR_TX, &cfg);
    printk("i2s_configure ret=%d\n", ret);
    if (ret) return 0;

    uint32_t phase = 0;

    /* Prime TX queue */
    for (int n = 0; n < 4; n++) {
        void *block = NULL;
        ret = k_mem_slab_alloc(&tx_slab, &block, K_FOREVER);
        if (ret) {
            printk("slab_alloc prime ret=%d\n", ret);
            return 0;
        }

        fill_block((int16_t *)block, &phase);

        ret = i2s_write(i2s, block, BLOCK_SIZE);
        printk("i2s_write(prime)[%d] ret=%d\n", n, ret);
        if (ret) return 0;
    }

    ret = i2s_trigger(i2s, I2S_DIR_TX, I2S_TRIGGER_START);
    printk("i2s_start ret=%d\n", ret);
    if (ret) return 0;

    printk("RUNNING. BCK=P0.11 LRCK=P0.12 SDOUT=P0.10\n");

    while (1) {
        /* 1) Allocate + fill a new TX block */
        void *block = NULL;
        ret = k_mem_slab_alloc(&tx_slab, &block, K_FOREVER);
        if (ret) {
            continue;
        }

        fill_block((int16_t *)block, &phase);

        /* 2) Queue it (this should now block up to timeout instead of -35 spam) */
        ret = i2s_write(i2s, block, BLOCK_SIZE);
        if (ret) {
            printk("i2s_write ret=%d\n", ret);
            k_mem_slab_free(&tx_slab, block);
            k_msleep(5);
            continue;
        }

        /* 3) Reclaim ONE completed TX block and free it back to the slab */
        void *released = NULL;
        size_t released_size = 0;

        ret = i2s_read(i2s, &released, &released_size);
        if (ret == 0 && released != NULL) {
            k_mem_slab_free(&tx_slab, released);
        }
        /* If ret != 0, no completed block ready yet — that's ok. */
    }
}