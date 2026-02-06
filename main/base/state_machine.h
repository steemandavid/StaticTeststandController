#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>

/* State definitions */
typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_ARMED,
    STATE_STARTTEST,
    STATE_IGNITION,
    STATE_TESTRUNNING,
    STATE_ENDTEST,
    STATE_HALT,
    STATE_CHECK_IGNITER,
    STATE_CHECK_BREAKWIRES,
    STATE_CALIBRATE_LOADCELL,
    STATE_CALIBRATE_PRESSURE,
    STATE_WELCOME_SCREEN,
    STATE_MAX
} base_state_t;

/* State machine API - to be implemented */
void state_machine_init(void);
void state_machine_task(void *pvParameters);
base_state_t state_machine_get_state(void);
const char *state_machine_get_name(base_state_t state);

#endif /* STATE_MACHINE_H */
