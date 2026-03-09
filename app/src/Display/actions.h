#ifndef ACTIONS_H
#define ACTIONS_H

void load_song_menu(void);
void menu_move_down(void);
void menu_move_up(void);
void menu_process_requests(void);
const char *menu_get_selected_song(void);
int menu_get_selected_index(void);

void ui_show_play_icon(void);
void ui_show_pause_icon(void);

#endif