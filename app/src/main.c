#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdint.h>

#include "test_wav.h"
#include "storage_init.h"

#define WAV_PATH "/RAM:/test.wav"


/* ---- little-endian helpers ---- */
static uint16_t u16le(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32le(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

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




static int parse_wav(const char *path)
{
    struct fs_file_t f;
    fs_file_t_init(&f);

    // Opens file
    int rc = fs_open(&f, path, FS_O_READ);
    if (rc < 0) {
        printk("fs_open() failed, rc=%d\n", rc);
        return rc;
    }

    // Reads the first 12 bytes of data from file and puts it into hdr
    // Bytes 0, 1, 2, 3 represent the Chunk ID (RIFF for a pcm WAV file)
    // Bytes 4, 5, 6, 7 represent the Chunk Size
    // Bytes 8, 9, 10, 11 represent format (WAVE in this case)

    uint8_t hdr[12];
    ssize_t r = fs_read(&f, hdr, sizeof(hdr));
    if (r != sizeof(hdr)) {
        printk("read header failed r=%d\n", (int)r);
        fs_close(&f);
        return -EIO;
    }

    // Confirms that files read are of WAV format (RIFF Chunk Id and WAVE format)
    if (memcmp(&hdr[0], "RIFF", 4) != 0 || memcmp(&hdr[8], "WAVE", 4) != 0) {
        printk("Not a RIFF/WAVE file\n");
        fs_close(&f);
        return -EINVAL;
    }

    bool got_fmt = false;
    bool got_data = false;

    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    uint32_t data_size = 0;
    int64_t data_offset = -1;

    while (1) {
        uint8_t chdr[8];
        r = fs_read(&f, chdr, sizeof(chdr));
        if (r == 0) break;
        if (r != sizeof(chdr)) {
            printk("chunk header short r=%d\n", (int)r);
            break;
        }

        char id[5] = { (char)chdr[0], (char)chdr[1], (char)chdr[2], (char)chdr[3], 0 };
        uint32_t chunk_size = u32le(&chdr[4]);

        if (memcmp(id, "fmt ", 4) == 0) {
            uint8_t fmt[32];
            if (chunk_size < 16 || chunk_size > sizeof(fmt)) {
                printk("fmt chunk size weird: %u\n", (unsigned)chunk_size);
                fs_close(&f);
                return -EINVAL;
            }

            r = fs_read(&f, fmt, chunk_size);
            if (r != (ssize_t)chunk_size) {
                printk("fmt read short r=%d\n", (int)r);
                fs_close(&f);
                return -EIO;
            }

            audio_format    = u16le(&fmt[0]);
            num_channels    = u16le(&fmt[2]);
            sample_rate     = u32le(&fmt[4]);
            bits_per_sample = u16le(&fmt[14]);

            got_fmt = true;
        } else if (memcmp(id, "data", 4) == 0) {
            data_offset = fs_tell(&f);
            data_size = chunk_size;

            rc = fs_seek(&f, (off_t)chunk_size, FS_SEEK_CUR);
            if (rc < 0) {
                printk("seek over data rc=%d\n", rc);
                fs_close(&f);
                return rc;
            }

            got_data = true;
        } else {
            rc = fs_seek(&f, (off_t)chunk_size, FS_SEEK_CUR);
            if (rc < 0) {
                printk("seek over chunk '%s' rc=%d\n", id, rc);
                fs_close(&f);
                return rc;
            }
        }

        if (chunk_size & 1) {
            rc = fs_seek(&f, 1, FS_SEEK_CUR);
            if (rc < 0) {
                printk("seek padding rc=%d\n", rc);
                fs_close(&f);
                return rc;
            }
        }

        if (got_fmt && got_data) break;
    }

    fs_close(&f);

    if (!got_fmt || !got_data) {
        printk("WAV missing fmt or data chunk (fmt=%d data=%d)\n", got_fmt, got_data);
        return -EINVAL;
    }

    printk("WAV OK:\n");
    printk("  audio_format=%u (1=PCM)\n", audio_format);
    printk("  channels=%u\n", num_channels);
    printk("  sample_rate=%u\n", sample_rate);
    printk("  bits_per_sample=%u\n", bits_per_sample);
    printk("  data_offset=%lld\n", (long long)data_offset);
    printk("  data_size=%u bytes\n", data_size);

    if (audio_format != 1) {
        printk("Not PCM (audio_format=%u)\n", audio_format);
        return -ENOTSUP;
    }

    return 0;
}

int main(void)
{
    int rc;

    printk("RAM FATFS WAV test start\n");

    /* 1) storage init (disk init + mkfs + mount) */
    rc = storage_init();
    if (rc != 0) {
        printk("storage_init failed rc=%d\n", rc);
        return 0;
    }

    /* 2) write embedded WAV into a real file */
    rc = write_file_from_bytes(WAV_PATH, test_wav, sizeof(test_wav));
    printk("write wav rc=%d (len=%u)\n", rc, (unsigned)sizeof(test_wav));
    if (rc != 0) return 0;

    /* 3) parse it back via fs_read/fs_seek */
    rc = parse_wav(WAV_PATH);
    printk("parse_wav rc=%d\n", rc);

    return 0;
}
