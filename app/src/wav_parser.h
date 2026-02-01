#ifndef WAV_PARSE_H
#define WAV_PARSE_H

#include <stdint.h>
#include <sys/types.h>

struct wav_info {
    uint16_t audio_format;     // 1 = PCM
    uint16_t num_channels;     // 1 mono, 2 stereo
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    off_t    data_offset;      // where PCM data starts
    uint32_t data_size;        // size of PCM data
};

int parse_wav(const char *path, struct wav_info *info);

#endif