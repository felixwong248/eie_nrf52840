#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include "LED.h"
#include "BTN.h"
#include "state_machine.h"


// State functions
static void first_string_entry(void *o);
static enum smf_state_result first_string_run(void *o);

static void second_string_entry(void *o);
static enum smf_state_result second_string_run(void *o);

static void print_string_entry(void *o);
static enum smf_state_result print_string_run(void *o);

static void standby_entry(void *o);
static enum smf_state_result standby_run(void *o);



enum led_state_machine_states {FIRST_STRING, SECOND_STRING, PRINT_STRING, STANDBY};

typedef struct {
    struct smf_ctx ctx;
    uint16_t count;
    uint16_t passcode;
} state_object;

static state_object lesson_6_sm;

/*----------------------------------------------------------
 * Local Variables
 *----------------------------------------------------------*/
static const struct smf_state states[] = {
    [FIRST_STRING] =    SMF_CREATE_STATE(first_string_entry, first_string_run, NULL, NULL, NULL),
    [SECOND_STRING] =   SMF_CREATE_STATE(second_string_entry, second_string_run, NULL, NULL, NULL),
    [PRINT_STRING] =    SMF_CREATE_STATE(print_string_entry, print_string_run, NULL, NULL, NULL),
    [STANDBY] =         SMF_CREATE_STATE(standby_entry, standby_run, NULL, NULL, NULL)
};

void state_machine_init(void)
{
    lesson_6_sm.count = 0;
    lesson_6_sm.passcode = 0;
    smf_set_initial(SMF_CTX(&lesson_6_sm), &states[FIRST_STRING]); // sets o to point at lesson_6_sm state machine (*o),
                                                                   // and sets first state to be FIRST_STRING
}

int state_machine_run(void)
{
    return smf_run_state(SMF_CTX(&lesson_6_sm));
}



// FIRST_STRING FUNCTIONS
static void first_string_entry(void *o){
    printk("ENTERED FIRST_STRING\n");
}

static enum smf_state_result first_string_run(void *o){

    if(BTN_check_clear_pressed(BTN2)){
        printk("NEXT STATE\n");
        smf_set_state(SMF_CTX(&lesson_6_sm), &states[SECOND_STRING]);
    }

    if(BTN_is_pressed(BTN0) && BTN_is_pressed(BTN1)){
        lesson_6_sm.count++;
        if ((lesson_6_sm.count % 100) == 0) {
            printk("hold count=%u\n", lesson_6_sm.count);
        }

        if (lesson_6_sm.count > 1500) {
            lesson_6_sm.count = 0;
            BTN_clear_pressed(BTN0);
            BTN_clear_pressed(BTN1);
            smf_set_state(SMF_CTX(&lesson_6_sm), &states[STANDBY]);
        }

    }
    else lesson_6_sm.count = 0;
    return SMF_EVENT_HANDLED;
}

// SECOND_STRING FUNCTIONS
static void second_string_entry(void *o){
    printk("ENTERED SECOND_STRING\n");
}

static enum smf_state_result second_string_run(void *o){
    if(BTN_check_clear_pressed(BTN2)){
        printk("NEXT STATE\n");
        smf_set_state(SMF_CTX(&lesson_6_sm), &states[PRINT_STRING]);
    }
    return SMF_EVENT_HANDLED;
}

// PRINT_STRING FUNCTIONS
static void print_string_entry(void *o){
    printk("ENTERED PRINT_STRING\n");
}

static enum smf_state_result print_string_run(void *o){
    if(BTN_check_clear_pressed(BTN2)){
        printk("NEXT STATE\n");
        smf_set_state(SMF_CTX(&lesson_6_sm), &states[STANDBY]);
    }
    return SMF_EVENT_HANDLED;
}
// STANDYBY FUNCTIONS
static void standby_entry(void *o){
    printk("ENTERED STANDBY\n");
}

static enum smf_state_result standby_run(void *o){
    if(BTN_check_clear_pressed(BTN0) | BTN_check_clear_pressed(BTN1) | BTN_check_clear_pressed(BTN2) |BTN_check_clear_pressed(BTN3)){
        printk("LEAVING STANDBY\n");
        smf_set_state(SMF_CTX(&lesson_6_sm), &states[FIRST_STRING]);
    }
    return SMF_EVENT_HANDLED;
}