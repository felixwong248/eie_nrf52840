#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include "LED.h"
#include "BTN.h"
#include "state_machine.h"

enum state_machine_states {FIRST_STRING, SECOND_STRING, PRINT_STRING, STANDBY};

typedef struct {
    struct smf_ctx ctx;
    uint16_t count;

    uint8_t passcode;
    int8_t bit_index;

    uint8_t duty_cycle;
    int8_t direction;
    enum state_machine_states previous_state;
} state_object;

static state_object lesson_6_sm;

// Function Prototypes
static void first_string_entry(void *o);
static enum smf_state_result first_string_run(void *o);

static void second_string_entry(void *o);
static enum smf_state_result second_string_run(void *o);

static void print_string_entry(void *o);
static enum smf_state_result print_string_run(void *o);

static void standby_entry(void *o);
static enum smf_state_result standby_run(void *o);
static void standby_exit(void *o);

static int standby_checker_function(state_object *sm, enum state_machine_states current_state);

static void passcode_add_bit(state_object *sm, uint8_t bit);
static void passcode_clear(state_object *sm);
static void Flash_LED(led_id led);

/*----------------------------------------------------------
 * Local Variables
 *----------------------------------------------------------*/
static const struct smf_state states[] = {
    [FIRST_STRING] =    SMF_CREATE_STATE(first_string_entry, first_string_run, NULL, NULL, NULL),
    [SECOND_STRING] =   SMF_CREATE_STATE(second_string_entry, second_string_run, NULL, NULL, NULL),
    [PRINT_STRING] =    SMF_CREATE_STATE(print_string_entry, print_string_run, NULL, NULL, NULL),
    [STANDBY] =         SMF_CREATE_STATE(standby_entry, standby_run, standby_exit, NULL, NULL)
};


void state_machine_init(void)
{
    lesson_6_sm.count = 0;

    lesson_6_sm.passcode = 0;
    lesson_6_sm.bit_index = 7;

    lesson_6_sm.duty_cycle = 0;
    lesson_6_sm.direction = 1;
    smf_set_initial(SMF_CTX(&lesson_6_sm), &states[FIRST_STRING]); // sets o to point at lesson_6_sm state machine (*o),
                                                                   // and sets first state to be FIRST_STRING

    lesson_6_sm.previous_state = FIRST_STRING;
}

int state_machine_run(void)
{
    return smf_run_state(SMF_CTX(&lesson_6_sm));
}


// FIRST_STRING FUNCTIONS
static void first_string_entry(void *o){
    state_object *sm = o;
    passcode_clear(sm);
    printk("ENTERED FIRST_STRING\n");
    LED_blink(LED3, LED_1HZ);

}

static enum smf_state_result first_string_run(void *o){

    state_object *sm = o;

    if(standby_checker_function(sm, FIRST_STRING)){
        return SMF_EVENT_HANDLED;
    }

    // Reset entered bits
    if (BTN_check_clear_pressed(BTN2)) {
        printk("RESET PASSCODE\n");
        passcode_clear(sm);
        return SMF_EVENT_HANDLED;
    }
    
    if(BTN_check_clear_pressed(BTN0)){          // adds 0 bit to passcode
        passcode_add_bit(sm, 0);
    } else if(BTN_check_clear_pressed(BTN1)){   // adds 1 bit to passcode
        passcode_add_bit(sm, 1);
    }

    if(BTN_check_clear_pressed(BTN3)){
        if(sm->bit_index<0){
        smf_set_state(SMF_CTX(sm), &states[SECOND_STRING]);
        }
    }

    return SMF_EVENT_HANDLED;
}

// SECOND_STRING FUNCTIONS
static void second_string_entry(void *o){
    printk("ENTERED SECOND_STRING\n");
    LED_blink(LED3, LED_4HZ);
}

static enum smf_state_result second_string_run(void *o){

    state_object *sm = o;

    if(standby_checker_function(sm, SECOND_STRING)){
        return SMF_EVENT_HANDLED;
    }

    if(BTN_check_clear_pressed(BTN2)){
        smf_set_state(SMF_CTX(sm), &states[PRINT_STRING]);
    }
    return SMF_EVENT_HANDLED;
}

// PRINT_STRING FUNCTIONS
static void print_string_entry(void *o){
    printk("ENTERED PRINT_STRING\n");
    LED_blink(LED3, LED_16HZ);
}

static enum smf_state_result print_string_run(void *o){

    state_object *sm = o;

    if(standby_checker_function(sm, PRINT_STRING)){
        return SMF_EVENT_HANDLED;
    }

    if(BTN_check_clear_pressed(BTN2)){
        smf_set_state(SMF_CTX(sm), &states[FIRST_STRING]);
    }

    if(BTN_check_clear_pressed(BTN3)){
        //printk("Code is %d \n", );
        printk("Printed passcode\n");
    }
        
    return SMF_EVENT_HANDLED;
}
// STANDYBY FUNCTIONS
static void standby_entry(void *o){
    printk("ENTERED STANDBY\n");
    state_object *sm = o;
    sm->duty_cycle = 0;
    sm->direction = 1;
}

static enum smf_state_result standby_run(void *o){

    state_object *sm = o;
    
    sm->duty_cycle += sm->direction;

    if (sm->duty_cycle >= 100) {
        sm->duty_cycle = 100;
        sm->direction = -1;
    } else if (sm->duty_cycle <= 0) {
        sm->duty_cycle = 0;
        sm->direction = 1;
    }

    LED_pwm(LED0, sm->duty_cycle);
    LED_pwm(LED1, sm->duty_cycle);
    LED_pwm(LED2, sm->duty_cycle);
    LED_pwm(LED3, sm->duty_cycle);

    if(BTN_check_clear_pressed(BTN0) || BTN_check_clear_pressed(BTN1) || BTN_check_clear_pressed(BTN2) || BTN_check_clear_pressed(BTN3)){
        smf_set_state(SMF_CTX(sm), &states[sm->previous_state]);
    }
    return SMF_EVENT_HANDLED;
}

static void standby_exit(void *o){
    LED_set(LED0, LED_OFF);
    LED_set(LED1, LED_OFF);
    LED_set(LED2, LED_OFF);
    LED_set(LED3, LED_OFF);
    
    printk("LEAVING STANDBY\n");
}

static int standby_checker_function(state_object *sm, enum state_machine_states current_state){
    
    if(BTN_is_pressed(BTN0) && BTN_is_pressed(BTN1)){
        sm->count++;
        if ((sm->count % 100) == 0) {
            printk("hold count=%u\n", sm->count);
        }

        if (sm->count > 1500) {
            sm->count = 0;
            sm->previous_state = current_state; // stores the current state that you were previously in 
                                                // so you can return to it after standby is over
            BTN_clear_pressed(BTN0);
            BTN_clear_pressed(BTN1);
            smf_set_state(SMF_CTX(sm), &states[STANDBY]);
            return 1;
        }

    }
    else sm->count = 0;
    
    return 0;
}


static void passcode_add_bit(state_object *sm, uint8_t bit){
    if(sm->bit_index < 0){
        printk("8 bit limit reached\n");
        return;
    } else if(bit) {
        sm->passcode |= (1 << sm->bit_index);
        printk("bit=1, index=%d\n", sm->bit_index);
        Flash_LED(LED1);
    } else {
        printk("bit=0, index=%d\n", sm->bit_index);
        Flash_LED(LED0);
    }
    sm->bit_index -= 1;
    return;
}

static void passcode_clear(state_object *sm){
    sm->passcode = 0;
    sm->bit_index = 7;
}

static void Flash_LED(led_id led){
    LED_set(led, LED_ON);
    k_msleep(100);
    LED_set(led, LED_OFF);
}
