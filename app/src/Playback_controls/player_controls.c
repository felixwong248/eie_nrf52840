#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "player_controls.h"
#include "storage_init.h"
#include "wav_parser.h"
#include "stream_wav_pcm.h"
#include "global_variables.h"

static int file_count = 0;
static int current_file_index = 0;
static char file_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
static char current_path[128];
static struct wav_info info;

// function increments file index to the next song
static void player_next(void)
{
    current_file_index++;
    if (current_file_index >= file_count) {
        current_file_index = 0; // cycles back to index 0
    }
}

// function decrements file index
static void player_prev(void)
{
    if (current_file_index == 0) {
        current_file_index = file_count - 1;
    } else {
        current_file_index--;
    }
}

int player_control_init(void)
{
    file_count = file_name_read(file_names); // gets # of files

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

    build_wav_path(current_path, sizeof(current_path), file_names[current_file_index]);

    // lists the files in the sd card
    for (int i = 0; i < file_count; i++) {
        printk("%d: %s\n", i, file_names[i]);
    }

    printk("------------------------\n");
    printk("Done listing files\n");

    return 0;
}

void player_control_run(void)
{
    int rc;

    while (1)
    {
        build_wav_path(current_path, sizeof(current_path), file_names[current_file_index]);
        printk("Playing file: %s\n", current_path);

        g_next_requested = false;
        g_prev_requested = false;
        g_stop_requested = false;

        rc = play_current_file(current_path, &info);
        printk("play_current_file rc=%d\n", rc);

        if (g_next_requested) {
            player_next();
        }
        else if (g_prev_requested) {
            player_prev();
        }
        else if (g_stop_requested) {
            printk("Playback stopped\n");
        }
        else {

            player_next(); // goes to next song if no flags become true when the play_current_file returns
        }

        k_sleep(K_MSEC(10));
    }
}