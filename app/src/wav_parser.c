#include <zephyr/sys/printk.h>
#include <zephyr/fs/fs.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "wav_parser.h"


// Functions to utilize little endian data
static uint16_t u16little_en(const uint8_t *p) { return (uint16_t)p[0] |
                                                 ((uint16_t)p[1] << 8); }

static uint32_t u32little_en(const uint8_t *p) { return (uint32_t)p[0] | 
                                                 ((uint32_t)p[1] << 8)  | 
                                                 ((uint32_t)p[2] << 16) | 
                                                 ((uint32_t)p[3] << 24); }


int parse_wav(const char *path, struct wav_info *info)
{
    struct fs_file_t f;
    fs_file_t_init(&f);

    // Opens file
    int rc = fs_open(&f, path, FS_O_READ);
    if (rc < 0) {
        printk("fs_open() failed, rc=%d\n", rc);
        return rc;
    }

    // Reads the first 12 bytes of data from file and puts it into riff_buff
    // Bytes 0, 1, 2, 3 represent the Chunk ID (RIFF for a pcm WAV file)
    // Bytes 4, 5, 6, 7 represent the Chunk Size
    // Bytes 8, 9, 10, 11 represent format (WAVE in this case)

    uint8_t riff_buff[12];
    ssize_t r = fs_read(&f, riff_buff, sizeof(riff_buff));
    if (r != sizeof(riff_buff)) {
        printk("read header failed r=%d\n", (int)r);
        fs_close(&f);
        return -EIO;
    }

    // Confirms that files read are of WAV format (RIFF Chunk Id and WAVE format)
    if (memcmp(&riff_buff[0], "RIFF", 4) != 0 || memcmp(&riff_buff[8], "WAVE", 4) != 0) {
        printk("Not a RIFF/WAVE file\n");
        fs_close(&f);
        return -EINVAL;
    }

    bool got_fmt = false;
    bool got_data = false;

    while (1) {
        
        // After confirming WAV file, move onto "fmt" sub-chunk section, which describes song information
        // Depending on the file, there might be "junk" bytes padded in to create a bigger byte offset. Create
        // the chunk_buff to read the sub chunk IDs and chunk size, so if there is chunk, we can skip it
        
        uint8_t chunk_buff[8];
        r = fs_read(&f, chunk_buff, sizeof(chunk_buff));
        if (r == 0) {
            break; // if theres no bytes being read, break loop
        }

        if (r != sizeof(chunk_buff)) {
            printk("chunk header short r=%d\n", (int)r);
            break; // if theres not enough bytes to specify ID and size, break loop
        }

        // store subchunk ID, the main goal of this section is to find fmt sub chunk, which gives us the song format data
        char sub_chunk_id[5];
        memcpy(sub_chunk_id, chunk_buff, 4);
        sub_chunk_id[4] = '\0';

        uint32_t chunk_size = u32little_en(&chunk_buff[4]); // subchunk size is in little endian order (4 bytes)

        if (memcmp(sub_chunk_id, "fmt ", 4) == 0) {
            uint8_t fmt_buff[32];
            if (chunk_size < 16 || chunk_size > sizeof(fmt_buff)) { // flags error when less than minimum subchunk size (16 bytes for standard PCM)
                printk("fmt chunk size error: %u\n", (unsigned)chunk_size);
                fs_close(&f);
                return -EINVAL;
            }

            r = fs_read(&f, fmt_buff, chunk_size);
            if (r != (ssize_t)chunk_size) {
                printk("fmt read short r=%d\n", (int)r);
                fs_close(&f);
                return -EIO;
            }

            // all of this data is little endian, so call function to convert them
            info->audio_format    = u16little_en(&fmt_buff[0]);
            info->num_channels    = u16little_en(&fmt_buff[2]);
            info->sample_rate     = u32little_en(&fmt_buff[4]);
            info->byte_rate       = u32little_en(&fmt_buff[8]);
            info->block_align     = u16little_en(&fmt_buff[12]);
            info->bits_per_sample = u16little_en(&fmt_buff[14]);

            got_fmt = true;
        } else if (memcmp(sub_chunk_id, "data", 4) == 0) { // data subchunk contains actual song
            info->data_offset = fs_tell(&f); // fs_tell gives current position of file pointer
            info->data_size = chunk_size;

            rc = fs_seek(&f, (off_t)chunk_size, FS_SEEK_CUR);
            if (rc < 0) {
                printk("seek over data rc=%d\n", rc);
                fs_close(&f);
                return rc;
            }

            got_data = true;

        } else {
            rc = fs_seek(&f, (off_t)chunk_size, FS_SEEK_CUR); // skips junk subchunk
            if (rc < 0) {
                printk("seek over chunk '%s' rc=%d\n", sub_chunk_id, rc);
                fs_close(&f);
                return rc;
            }
        }

        if (chunk_size & 1) { // If chunk is offset by an odd byte, shifts back to even offset
            rc = fs_seek(&f, 1, FS_SEEK_CUR);
            if (rc < 0) {
                printk("failed to skip 1 byte padding =%d\n", rc);
                fs_close(&f);
                return rc;
            }
        }

        if (got_fmt && got_data) break;
    }

    fs_close(&f);

    if (!got_fmt || !got_data) {
        printk("WAV missing fmt or data chunk\n");
        return -EINVAL;
    }

    printk("WAV OK:\n\n");
    printk("audio_format=%u (1=PCM)\n", info->audio_format);
    printk("channels=%u\n", info->num_channels);
    printk("sample_rate=%u\n", info->sample_rate);
    printk("byte rate=%u\n", info->byte_rate);
    printk("block align=%u\n", info->block_align);
    printk("bits_per_sample=%u\n", info->bits_per_sample);
    printk("data_offset=%lld\n", (long long)info->data_offset);
    printk("data_size=%u bytes\n\n", info->data_size);

    if (info->audio_format != 1) {
        printk("Not PCM (audio_format=%u)\n", info->audio_format);
        return -ENOTSUP;
    }

    return 0;
}