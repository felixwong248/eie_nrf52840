#include "actions.h"
#include "vars.h"
#include "ui.h"
#include "storage_init.h"
#include "global_variables.h"
#include "player_controls.h"
#include "images.h"

#include <string.h>
#include <stdbool.h>

#define VISIBLE_ROWS 4

char g_song_names[MAX_FILE_AMOUNT][MAX_LETTER_AMOUNT];
int g_song_count = 0;
int g_selected_index = 0;
int g_top_index = 0;

static lv_obj_t *get_row_obj(int row)
{
    lv_obj_t *rows[VISIBLE_ROWS] = {
        objects.songname1,
        objects.songname2,
        objects.songname3,
        objects.songname4
    };

    return rows[row];
}

static void redraw_visible_song_text(void)
{
    for (int row = 0; row < VISIBLE_ROWS; row++) {
        int song_index = g_top_index + row;
        lv_obj_t *row_obj = get_row_obj(row);

        if (song_index < g_song_count) {
            lv_textarea_set_text(row_obj, g_song_names[song_index]);
            lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_textarea_set_text(row_obj, "");
            lv_obj_add_flag(row_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void update_selection_box(void)
{
    int visible_row = g_selected_index - g_top_index;

    if (g_song_count == 0 || visible_row < 0 || visible_row >= VISIBLE_ROWS) {
        lv_obj_add_flag(objects.border_select, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(objects.border_select, LV_OBJ_FLAG_HIDDEN);

    /* Adjust these if needed */
    lv_obj_set_pos(objects.border_select, 35, 63 + (visible_row * 39));
    lv_obj_set_size(objects.border_select, 250, 36);
}

void load_song_menu(void)
{
    g_song_count = file_name_read(g_song_names);

    if (g_song_count < 0) {
        g_song_count = 0;
    }

    g_selected_index = 0;
    g_top_index = 0;

    redraw_visible_song_text();
    update_selection_box();
    ui_show_play_icon();
}

void menu_move_down(void)
{
    bool changed = false;
    bool scrolled = false;

    if (g_song_count == 0) {
        return;
    }

    if (g_selected_index < g_song_count - 1) {
        g_selected_index++;
        changed = true;
    }

    if (g_selected_index >= g_top_index + VISIBLE_ROWS) {
        g_top_index++;
        scrolled = true;
    }

    if (scrolled) {
        redraw_visible_song_text();
    }

    update_selection_box();

    if (changed) {
        player_set_current_index(g_selected_index);
        g_stop_requested = true;
        ui_show_play_icon();
    }
}

void menu_move_up(void)
{
    bool changed = false;
    bool scrolled = false;

    if (g_song_count == 0) {
        return;
    }

    if (g_selected_index > 0) {
        g_selected_index--;
        changed = true;
    }

    if (g_selected_index < g_top_index) {
        g_top_index--;
        scrolled = true;
    }

    if (scrolled) {
        redraw_visible_song_text();
    }

    update_selection_box();

    if (changed) {
        player_set_current_index(g_selected_index);
        g_stop_requested = true;
        ui_show_play_icon();
    }
}

void menu_play_selected(void)
{
    if (g_song_count == 0) {
        return;
    }

    player_set_current_index(g_selected_index);
    g_play_requested = true;
    ui_show_pause_icon();
}

void menu_process_requests(void)
{
    if (g_menu_up_requested) {
        g_menu_up_requested = false;
        menu_move_up();
    }

    if (g_menu_down_requested) {
        g_menu_down_requested = false;
        menu_move_down();
    }

    if (g_menu_play_requested) {
        g_menu_play_requested = false;
        menu_play_selected();
    }
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

void ui_show_play_icon(void)
{
    lv_obj_clear_flag(objects.play_button, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(objects.pause_button, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_pause_icon(void)
{
    /* hide play icon */
    lv_obj_add_flag(objects.play_button, LV_OBJ_FLAG_HIDDEN);

    /* show pause icon */
    lv_obj_clear_flag(objects.pause_button, LV_OBJ_FLAG_HIDDEN);
}