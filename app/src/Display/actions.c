#include "actions.h"
#include "vars.h"
#include "ui.h"
#include "storage_init.h"

#include <string.h>

#define VISIBLE_ROWS 4

char g_song_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
int g_song_count = 0;
int g_selected_index = 0;
int g_top_index = 0;

static void redraw_song_rows(void)
{
    lv_obj_t *rows[VISIBLE_ROWS] = {
        objects.songname1,
        objects.songname2,
        objects.songname3,
        objects.songname4
    };

    for (int row = 0; row < VISIBLE_ROWS; row++) {
        int song_index = g_top_index + row;

        if (song_index < g_song_count) {
            lv_textarea_set_text(rows[row], g_song_names[song_index]);

            if (song_index == g_selected_index) {
                lv_obj_set_style_border_width(rows[row], 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_border_color(rows[row], lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
            } else {
                lv_obj_set_style_border_width(rows[row], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            }
        } else {
            lv_textarea_set_text(rows[row], "");
            lv_obj_set_style_border_width(rows[row], 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void load_song_menu(void)
{
    g_song_count = file_name_read(g_song_names);

    if (g_song_count < 0) {
        g_song_count = 0;
    }

    g_selected_index = 0;
    g_top_index = 0;

    redraw_song_rows();
}

void menu_move_down(void)
{
    if (g_song_count == 0) {
        return;
    }

    if (g_selected_index < g_song_count - 1) {
        g_selected_index++;
    }

    if (g_selected_index >= g_top_index + VISIBLE_ROWS) {
        g_top_index++;
    }

    redraw_song_rows();
}

void menu_move_up(void)
{
    if (g_song_count == 0) {
        return;
    }

    if (g_selected_index > 0) {
        g_selected_index--;
    }

    if (g_selected_index < g_top_index) {
        g_top_index--;
    }

    redraw_song_rows();
}

const char *menu_get_selected_song(void)
{
    if (g_song_count == 0) {
        return NULL;
    }

    return g_song_names[g_selected_index];
}

int menu_get_selected_index(void)
{
    return g_selected_index;
}