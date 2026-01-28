#ifndef WAV_PARSE_H
#define WAV_PARSE_H

#include <stdint.h>

struct wav_info {
    uint16_t audio_format;     // 1 = PCM
    uint16_t num_channels;     // 1 mono, 2 stereo
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    off_t    data_offset;      // where PCM bytes start
    uint32_t data_size;        // how many PCM bytes
};

int parse_wav();

#endif