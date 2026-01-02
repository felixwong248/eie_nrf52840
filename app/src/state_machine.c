#include <zephyr/smf.h>
#include "LED.h"
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
} led_state_object_t;

/*----------------------------------------------------------
 * Local Variables
 *----------------------------------------------------------*/
static const struct smf_state states[] = {
    [FIRST_STRING] =    SMF_CREATE_STATE(),
    [SECOND_STRING] =   SMF_CREATE_STATE(),
    [PRINT_STRING] =    SMF_CREATE_STATE(),
    [STANDBY] =         SMF_CREATE_STATE(),
}