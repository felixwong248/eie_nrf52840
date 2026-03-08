#include <zephyr/kernel.h>
#include "BTN.h"
#include "global_variables.h"
#include "button_control.h"

#define BUTTON_THREAD_STACK_SIZE 1024 
#define BUTTON_THREAD_PRIORITY 5 

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

    while (1) {
        if (BTN_check_clear_pressed(BTN2)) {
            g_next_requested = true;
        }

        if (BTN_check_clear_pressed(BTN3)) {
            g_prev_requested = true;
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