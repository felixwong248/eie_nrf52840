#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "player_controls.h"
#include "storage_init.h"
#include "wav_parser.h"
#include "stream_wav_pcm.h"
#include "global_variables.h"

#define PLAYER_THREAD_STACK_SIZE 4096
#define PLAYER_THREAD_PRIORITY   2

static int file_count = 0;
static int current_file_index = 0;
static char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
static char current_path[128];
static struct wav_info info;

K_THREAD_STACK_DEFINE(player_thread_stack, PLAYER_THREAD_STACK_SIZE);
static struct k_thread player_thread_data;

static void player_next(void)
{
    current_file_index++;
    if (current_file_index >= file_count) {
        current_file_index = 0;
    }
}

static void player_prev(void)
{
    if (current_file_index == 0) {
        current_file_index = file_count - 1;
    } else {
        current_file_index--;
    }
}

static void player_thread(void *p1, void *p2, void *p3)
{
    int rc;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        build_wav_path(current_path, sizeof(current_path), file_names[current_file_index]);
        printk("Playing file: %s\n", current_path);

        g_next_requested = false;
        g_prev_requested = false;
        g_stop_requested = false;

        rc = play_current_file(current_path, &info);
        printk("play_current_file rc=%d next=%d prev=%d stop=%d\n",
            rc, g_next_requested, g_prev_requested, g_stop_requested);

        if (g_next_requested) {
            printk("reason: next requested\n");
            player_next();
        }
        else if (g_prev_requested) {
            printk("reason: prev requested\n");
            player_prev();
        }
        else if (g_stop_requested) {
            printk("reason: stop requested\n");
        }
        else if (rc == 0) {
            printk("reason: normal end of song\n");
            player_next();
        } else {
        printk("Playback error, staying on current file\n");
        k_sleep(K_MSEC(200));
        }
    }
}
int player_control_init(void)
{
    file_count = file_name_read(file_names);

    if (file_count < 0) {
        printk("file_name_read failed\n");
        return file_count;
    }

    if (file_count == 0) {
        printk("No files found on SD card\n");
        return -1;
    }

    printk("\nFiles found on SD card:\n");
    printk("------------------------\n");

    for (int i = 0; i < file_count; i++) {
        printk("%d: %s\n", i, file_names[i]);
    }

    printk("------------------------\n");
    printk("Done listing files\n");

    return 0;
}

int player_control_start(void)
{
    k_thread_create(&player_thread_data,
                    player_thread_stack,
                    K_THREAD_STACK_SIZEOF(player_thread_stack),
                    player_thread,
                    NULL, NULL, NULL,
                    PLAYER_THREAD_PRIORITY,
                    0,
                    K_NO_WAIT);

    return 0;
}