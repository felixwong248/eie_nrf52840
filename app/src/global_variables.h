#ifndef GLOBAL_VAR_H
#define GLOBAL_VAR_H

#include <stdbool.h>

extern volatile bool g_next_requested;
extern volatile bool g_prev_requested;
extern volatile bool g_stop_requested;

extern volatile bool g_menu_up_requested;
extern volatile bool g_menu_down_requested;
extern volatile bool g_menu_play_requested;

extern volatile bool g_play_requested;
#endif