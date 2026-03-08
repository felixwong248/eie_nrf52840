#-------------------------------------------------------------------------------
# Zephyr Example Application
#
# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

zephyr_include_directories(src)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/storage_init.c)
target_sources(app PRIVATE src/wav_parser.c)
target_sources(app PRIVATE src/stream_wav_pcm.c)
target_sources(app PRIVATE src/global_variables.c)
target_sources(app PRIVATE src/button_control.c)
target_sources(app PRIVATE src/player_controls.c)