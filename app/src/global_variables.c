#include "global_variables.h"

// global variables/flags to use for multithreading for player controls
volatile bool g_next_requested = false;
volatile bool g_prev_requested = false;
volatile bool g_stop_requested = false;

volatile bool g_menu_up_requested = false;
volatile bool g_menu_down_requested = false;
volatile bool g_menu_play_requested = false;

volatile bool g_play_requested = false;