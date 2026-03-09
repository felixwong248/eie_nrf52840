#include <zephyr/kernel.h>
#include "BTN.h"
#include "global_variables.h"
#include "button_control.h"

#define BUTTON_THREAD_STACK_SIZE 1024 
#define BUTTON_THREAD_PRIORITY 5 
#define LONG_PRESS_COUNT 80

// this part reserves board memory for the thread stack
K_THREAD_STACK_DEFINE(button_thread_stack, BUTTON_THREAD_STACK_SIZE);
static struct k_thread button_thread_data;


static void button_thread(void *p1, void *p2, void *p3)
{
    // standard thread format for zephyr needs to have 3 args
    // here we don't need it so just feed NULL into the k_create_thread function (p1,p2,p3)
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int btn2_hold_count = 0;
    int btn3_hold_count = 0;

    bool btn2_long_press_fired = false;
    bool btn3_long_press_fired = false;

    while (1) {
        /* BTN2 */
        if (BTN_is_pressed(BTN2)) {
            btn2_hold_count++;

            if (btn2_hold_count > LONG_PRESS_COUNT && !btn2_long_press_fired) {
                g_menu_play_requested = true;
                btn2_long_press_fired = true;
            }
        } else {
            if (btn2_hold_count > 0 && !btn2_long_press_fired) {
                g_menu_up_requested = true;
            }

            btn2_hold_count = 0;
            btn2_long_press_fired = false;
        }

        /* BTN3 */
        if (BTN_is_pressed(BTN3)) {
            btn3_hold_count++;

            if (btn3_hold_count > LONG_PRESS_COUNT && !btn3_long_press_fired) {
                g_menu_play_requested = true;
                btn3_long_press_fired = true;
            }
        } else {
            if (btn3_hold_count > 0 && !btn3_long_press_fired) {
                g_menu_down_requested = true;
            }

            btn3_hold_count = 0;
            btn3_long_press_fired = false;
        }

        k_sleep(K_MSEC(10));
    }
}

int button_control_init(void)
{
    int rc;

    rc = BTN_init_selected(BTN2);
    if (rc != 0) {
        return rc;
    }

    rc = BTN_init_selected(BTN3);
    if (rc != 0) {
        return rc;
    }

    k_thread_create(&button_thread_data, button_thread_stack, K_THREAD_STACK_SIZEOF(button_thread_stack),
                    button_thread, NULL, NULL, NULL, BUTTON_THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}