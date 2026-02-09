#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

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

/* State machine API */
void state_machine_init(void);
void state_machine_task(void *pvParameters);
base_state_t state_machine_get_state(void);
const char *state_machine_get_name(base_state_t state);

/* Test interface - force state transitions for testing */
base_state_t state_machine_from_name(const char *name);
esp_err_t state_machine_force_state(base_state_t new_state);

/* Test mode control - disable automatic HALT transitions during testing */
void state_machine_set_test_mode(bool enabled);

#endif /* STATE_MACHINE_H */
