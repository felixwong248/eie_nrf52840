#pragma once
#include <stdint.h>

/*
 * Tiny valid PCM WAV:
 * - RIFF/WAVE
 * - fmt  chunk (PCM, mono, 8 kHz, 16-bit)
 * - data chunk (16 samples -> 32 bytes)
 *
 * This is just for parsing + plumbing tests.
 */
static const uint8_t test_wav[] = {
    /* RIFF header */
    'R','I','F','F',
    0x44,0x00,0x00,0x00,          /* ChunkSize = 68 (36 + data_size=32) */
    'W','A','V','E',

    /* fmt  chunk */
    'f','m','t',' ',
    0x10,0x00,0x00,0x00,          /* Subchunk1Size = 16 */
    0x01,0x00,                    /* AudioFormat = 1 (PCM) */
    0x01,0x00,                    /* NumChannels = 1 */
    0x40,0x1F,0x00,0x00,          /* SampleRate = 8000 */
    0x80,0x3E,0x00,0x00,          /* ByteRate = 8000 * 1 * 16/8 = 16000 */
    0x02,0x00,                    /* BlockAlign = 2 */
    0x10,0x00,                    /* BitsPerSample = 16 */

    /* data chunk */
    'd','a','t','a',
    0x20,0x00,0x00,0x00,          /* Subchunk2Size = 32 bytes */

    /* 16 samples (little-endian int16) */
    0x00,0x00, 0x10,0x00, 0x20,0x00, 0x30,0x00,
    0x40,0x00, 0x50,0x00, 0x60,0x00, 0x70,0x00,
    0x70,0x00, 0x60,0x00, 0x50,0x00, 0x40,0x00,
    0x30,0x00, 0x20,0x00, 0x10,0x00, 0x00,0x00,
};
