#ifndef STREAM_WAV_PCM_H
#define STREAM_WAV_PCM_H

#include "wav_parser.h"

int stream_pcm(const char *path, const struct wav_info *info);

#endif