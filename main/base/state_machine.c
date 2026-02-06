/*******************************************************************************
 * BASE Unit - State Machine
 *
 * Implements the core state machine for the BASE unit.
 * See FSD Section 4.1 for state definitions and transitions.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_BASE

#include "state_machine.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "safety.h"
#include "rgb_led.h"
#include "buzzer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "state_machine";

static volatile base_state_t s_current_state = STATE_INIT;

static const char *s_state_names[STATE_MAX] = {
    [STATE_INIT]                = "INIT",
    [STATE_IDLE]                = "IDLE",
    [STATE_ARMED]               = "ARMED",
    [STATE_STARTTEST]           = "STARTTEST",
    [STATE_IGNITION]            = "IGNITION",
    [STATE_TESTRUNNING]         = "RUNNING",
    [STATE_ENDTEST]             = "ENDTEST",
    [STATE_HALT]                = "HALT",
    [STATE_CHECK_IGNITER]       = "CHK_IGN",
    [STATE_CHECK_BREAKWIRES]    = "CHK_BRK",
    [STATE_CALIBRATE_LOADCELL]  = "CAL_LC",
    [STATE_CALIBRATE_PRESSURE]  = "CAL_PR",
    [STATE_WELCOME_SCREEN]      = "WELCOME",
};

/*******************************************************************************
 * Transition Validation Table
 ******************************************************************************/

static const base_state_t s_valid_transitions[STATE_MAX][8] = {
    [STATE_INIT]                = { STATE_WELCOME_SCREEN, STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_IDLE]                = { STATE_ARMED, STATE_CHECK_IGNITER, STATE_CHECK_BREAKWIRES,
                                    STATE_CALIBRATE_LOADCELL, STATE_CALIBRATE_PRESSURE,
                                    STATE_WELCOME_SCREEN, STATE_HALT, STATE_MAX },
    [STATE_ARMED]               = { STATE_STARTTEST, STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_STARTTEST]           = { STATE_IGNITION, STATE_HALT, STATE_MAX },
    [STATE_IGNITION]            = { STATE_TESTRUNNING, STATE_HALT, STATE_MAX },
    [STATE_TESTRUNNING]         = { STATE_ENDTEST, STATE_HALT, STATE_MAX },
    [STATE_ENDTEST]             = { STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_HALT]                = { STATE_INIT, STATE_MAX },
    [STATE_CHECK_IGNITER]       = { STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_CHECK_BREAKWIRES]    = { STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_CALIBRATE_LOADCELL]  = { STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_CALIBRATE_PRESSURE]  = { STATE_IDLE, STATE_HALT, STATE_MAX },
    [STATE_WELCOME_SCREEN]      = { STATE_IDLE, STATE_HALT, STATE_MAX },
};

static bool is_transition_valid(base_state_t from, base_state_t to)
{
    if (from >= STATE_MAX || to >= STATE_MAX) {
        return false;
    }
    for (int i = 0; s_valid_transitions[from][i] != STATE_MAX; i++) {
        if (s_valid_transitions[from][i] == to) {
            return true;
        }
    }
    return false;
}

/*******************************************************************************
 * State Handlers (stubs - filled by later features)
 ******************************************************************************/

typedef void (*state_handler_fn)(void);

typedef struct {
    state_handler_fn on_enter;
    state_handler_fn on_run;
    state_handler_fn on_exit;
} state_handlers_t;

/* Forward declaration — needed by handle_welcome_run */
static bool transition_to(base_state_t new_state);

static void handler_stub(void) { }

/* INIT */
static void handle_init_enter(void)
{
    ESP_LOGI(TAG, "Entering INIT state");
}

/* IDLE */
static void handle_idle_enter(void)
{
    ESP_LOGI(TAG, "Entering IDLE state");
    rgb_led_set(0, 255, 0, LED_PATTERN_BREATHING);
    buzzer_stop();
}

/* ARMED */
static void handle_armed_enter(void)
{
    ESP_LOGI(TAG, "Entering ARMED state");
    rgb_led_set(255, 165, 0, LED_PATTERN_SOLID);
    buzzer_play(BUZZER_PATTERN_SHORT_BEEP);

    /* Notify REMOTE: buzzer + button LED on */
    espnow_packet_t pkt = {0};
    pkt.command = CMD_BUZZER_ON;
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));

    pkt.command = CMD_BUTTON_LED_ON;
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));
}

static void handle_armed_exit(void)
{
    buzzer_stop();

    /* Notify REMOTE: buzzer + button LED off */
    espnow_packet_t pkt = {0};
    pkt.command = CMD_BUZZER_OFF;
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));

    pkt.command = CMD_BUTTON_LED_OFF;
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));
}

/* STARTTEST */
static void handle_starttest_enter(void)
{
    ESP_LOGW(TAG, "STARTTEST: ADC/igniter not ready, halting");
    /* Phase 2 ADC/igniter not implemented — immediately halt */
    uint8_t cmd = CMD_HALT;
    xQueueSendToFront(state_event_queue, &cmd, 0);
}

/* HALT */
static void handle_halt_enter(void)
{
    ESP_LOGW(TAG, "Entering HALT state");
    safety_enter_safe_state();
    rgb_led_set(255, 0, 0, LED_PATTERN_PULSE_HALF_HZ);
    buzzer_play(BUZZER_PATTERN_ALARM);
}

static void handle_halt_run(void)
{
    safety_watchdog_feed();
}

static void handle_halt_exit(void)
{
    buzzer_stop();
}

/* WELCOME_SCREEN */
static uint32_t s_welcome_ticks = 0;

static void handle_welcome_enter(void)
{
    ESP_LOGI(TAG, "Entering WELCOME_SCREEN state");
    rgb_led_set(0, 0, 255, LED_PATTERN_BREATHING);
    s_welcome_ticks = 0;

    /* Send display commands to REMOTE */
    espnow_packet_t pkt = {0};
    pkt.command = CMD_DISPLAY_CLEAR;
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));

    memset(&pkt, 0, sizeof(pkt));
    pkt.command = CMD_DISPLAY_LOG_LINE;
    strncpy(pkt.message, "Test Stand Ctrl v1.0", sizeof(pkt.message) - 1);
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));
}

static void handle_welcome_run(void)
{
    /* 100ms per tick (queue timeout in main loop), 20 ticks = 2000ms */
    s_welcome_ticks++;
    if (s_welcome_ticks >= 20) {
        transition_to(STATE_IDLE);
    }
}

static const state_handlers_t s_handlers[STATE_MAX] = {
    [STATE_INIT]                = { handle_init_enter,      handler_stub,       handler_stub },
    [STATE_IDLE]                = { handle_idle_enter,       handler_stub,       handler_stub },
    [STATE_ARMED]               = { handle_armed_enter,      handler_stub,       handle_armed_exit },
    [STATE_STARTTEST]           = { handle_starttest_enter,  handler_stub,       handler_stub },
    [STATE_IGNITION]            = { handler_stub,            handler_stub,       handler_stub },
    [STATE_TESTRUNNING]         = { handler_stub,            handler_stub,       handler_stub },
    [STATE_ENDTEST]             = { handler_stub,            handler_stub,       handler_stub },
    [STATE_HALT]                = { handle_halt_enter,       handle_halt_run,    handle_halt_exit },
    [STATE_CHECK_IGNITER]       = { handler_stub,            handler_stub,       handler_stub },
    [STATE_CHECK_BREAKWIRES]    = { handler_stub,            handler_stub,       handler_stub },
    [STATE_CALIBRATE_LOADCELL]  = { handler_stub,            handler_stub,       handler_stub },
    [STATE_CALIBRATE_PRESSURE]  = { handler_stub,            handler_stub,       handler_stub },
    [STATE_WELCOME_SCREEN]      = { handle_welcome_enter,    handle_welcome_run, handler_stub },
};

/*******************************************************************************
 * LED command mapping per state
 ******************************************************************************/

static uint8_t state_to_led_cmd(base_state_t state)
{
    switch (state) {
        case STATE_IDLE:
        case STATE_CHECK_IGNITER:
        case STATE_CHECK_BREAKWIRES:
        case STATE_CALIBRATE_LOADCELL:
        case STATE_CALIBRATE_PRESSURE:
        case STATE_WELCOME_SCREEN:
            return CMD_LED_SAFE;
        case STATE_ARMED:
        case STATE_STARTTEST:
            return CMD_LED_ARMED;
        case STATE_IGNITION:
        case STATE_TESTRUNNING:
            return CMD_LED_TESTING;
        case STATE_ENDTEST:
            return CMD_LED_COMPLETE;
        case STATE_HALT:
            return CMD_LED_ERROR;
        default:
            return CMD_LED_SAFE;
    }
}

static void broadcast_state_change(base_state_t new_state)
{
    espnow_packet_t pkt = {0};
    pkt.base_state = (uint8_t)new_state;
    pkt.command = state_to_led_cmd(new_state);
    strncpy(pkt.message, s_state_names[new_state], sizeof(pkt.message) - 1);
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));
}

static bool transition_to(base_state_t new_state)
{
    base_state_t old_state = s_current_state;

    if (!is_transition_valid(old_state, new_state)) {
        ESP_LOGW(TAG, "Invalid transition: %s -> %s",
                 s_state_names[old_state], s_state_names[new_state]);
        return false;
    }

    ESP_LOGI(TAG, "Transition: %s -> %s",
             s_state_names[old_state], s_state_names[new_state]);

    if (s_handlers[old_state].on_exit) {
        s_handlers[old_state].on_exit();
    }

    s_current_state = new_state;

    if (s_handlers[new_state].on_enter) {
        s_handlers[new_state].on_enter();
    }

    broadcast_state_change(new_state);
    return true;
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

void state_machine_init(void)
{
    s_current_state = STATE_INIT;
    ESP_LOGI(TAG, "State machine initialized");
}

base_state_t state_machine_get_state(void)
{
    return s_current_state;
}

const char *state_machine_get_name(base_state_t state)
{
    if (state >= STATE_MAX) {
        return "UNKNOWN";
    }
    return s_state_names[state];
}

void state_machine_task(void *pvParameters)
{
    uint8_t event;
    ESP_LOGI(TAG, "State machine task started");

    /* Run INIT entry */
    if (s_handlers[STATE_INIT].on_enter) {
        s_handlers[STATE_INIT].on_enter();
    }
    broadcast_state_change(STATE_INIT);

    /* Auto-transition INIT -> WELCOME_SCREEN */
    vTaskDelay(pdMS_TO_TICKS(100));
    transition_to(STATE_WELCOME_SCREEN);

    for (;;) {
        safety_watchdog_feed();

        if (s_handlers[s_current_state].on_run) {
            s_handlers[s_current_state].on_run();
        }

        if (xQueueReceive(state_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGD(TAG, "Event 0x%02x in state %s",
                     event, s_state_names[s_current_state]);

            if (event == CMD_HALT) {
                transition_to(STATE_HALT);

            } else if (event == CMD_COMMS_ERROR) {
                /* Comms lost: halt from any non-HALT state */
                if (s_current_state != STATE_HALT) {
                    ESP_LOGE(TAG, "COMMS_ERROR: transitioning to HALT");
                    transition_to(STATE_HALT);
                }

            } else if (event == CMD_SWITCH_TO_ARMED) {
                /* Only allow arming from IDLE */
                if (s_current_state == STATE_IDLE) {
                    transition_to(STATE_ARMED);
                } else {
                    ESP_LOGW(TAG, "ARM rejected: not in IDLE (current=%s)",
                             s_state_names[s_current_state]);
                }

            } else if (event == CMD_SWITCH_TO_SAFE) {
                switch (s_current_state) {
                    case STATE_ARMED:
                        /* Disarm */
                        transition_to(STATE_IDLE);
                        break;
                    case STATE_STARTTEST:
                    case STATE_IGNITION:
                    case STATE_TESTRUNNING:
                    case STATE_ENDTEST:
                        /* Emergency stop during test */
                        transition_to(STATE_HALT);
                        break;
                    default:
                        /* IDLE, check, calibrate, HALT — already safe or can't act */
                        break;
                }

            } else if (event == CMD_BTN_LONG_PRESS) {
                switch (s_current_state) {
                    case STATE_ARMED:
                        /* Deliberate long press to start test */
                        transition_to(STATE_STARTTEST);
                        break;
                    case STATE_CHECK_IGNITER:
                    case STATE_CHECK_BREAKWIRES:
                    case STATE_CALIBRATE_LOADCELL:
                    case STATE_CALIBRATE_PRESSURE:
                    case STATE_WELCOME_SCREEN:
                        transition_to(STATE_IDLE);
                        break;
                    case STATE_HALT:
                        transition_to(STATE_INIT);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

#endif /* BUILD_TARGET_BASE */
