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
#include "sd_logger.h"
#include "settings.h"
#include "adc_as1256.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "state_machine";

static volatile base_state_t s_current_state = STATE_INIT;
static volatile bool s_test_mode = false;  /* Flag to disable auto-HALT during testing */

/* Test flow state tracking */
static struct {
    bool test_active;
    uint64_t test_start_time_us;
    uint32_t test_sample_count;
    float baseline_thrust;
    float baseline_pressure;
    float max_thrust;
    float max_pressure;
    float total_impulse;
    uint32_t end_counter;
    uint32_t post_burn_counter;
    uint64_t last_sample_time_us;
} s_test_data = {0};

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

/* Generic run handler that feeds watchdog */
static void handle_generic_run(void)
{
    safety_watchdog_feed();
}

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

static void handle_idle_run(void)
{
    /* Feed watchdog to keep system alive while in IDLE */
    safety_watchdog_feed();
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
    ESP_LOGI(TAG, "Entering STARTTEST state");

    /* Get current settings */
    const settings_t *settings = settings_get();
    if (!settings) {
        ESP_LOGE(TAG, "No settings available, halting");
        uint8_t cmd = CMD_HALT;
        xQueueSendToFront(state_event_queue, &cmd, 0);
        return;
    }

    /* Initialize test data */
    memset(&s_test_data, 0, sizeof(s_test_data));
    s_test_data.test_active = true;
    s_test_data.test_start_time_us = esp_timer_get_time();

    /* Create CSV file with timestamp */
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%llu", s_test_data.test_start_time_us);

    esp_err_t ret = sd_logger_create_test_file(timestamp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create test file, halting");
        uint8_t cmd = CMD_HALT;
        xQueueSendToFront(state_event_queue, &cmd, 0);
        return;
    }

    ESP_LOGI(TAG, "Test file created, igniter_on_time=%.1f", settings->igniter_on_time);

    /* Transition to IGNITION after 1 second */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Send IGNITION command to self via queue */
    uint8_t cmd = CMD_IGNITION;
    xQueueSendToFront(state_event_queue, &cmd, 0);
}

/* HALT */
static void handle_halt_enter(void)
{
    ESP_LOGW(TAG, "Entering HALT state");
    safety_enter_safe_state();
    rgb_led_set(255, 0, 0, LED_PATTERN_PULSE_HALF_HZ);
    buzzer_play(BUZZER_PATTERN_LONG_BEEP);  /* Single long beep to indicate HALT, then silent */
}

static void handle_halt_run(void)
{
    safety_watchdog_feed();
}

static void handle_halt_exit(void)
{
    buzzer_stop();
}

/* CHECK_IGNITER - Pre-flight igniter check */
static void handle_chk_ign_enter(void)
{
    ESP_LOGI(TAG, "Entering CHK_IGN state");
    rgb_led_set(255, 0, 255, LED_PATTERN_PULSE_HALF_HZ);  /* Magenta pulsing */
}

/* CHECK_BREAKWIRES - Pre-flight breakwire check */
static void handle_chk_brk_enter(void)
{
    ESP_LOGI(TAG, "Entering CHK_BRK state");
    rgb_led_set(255, 0, 255, LED_PATTERN_PULSE_HALF_HZ);  /* Magenta pulsing */
}

/* CALIBRATE_LOADCELL - Load cell calibration mode */
static void handle_cal_lc_enter(void)
{
    ESP_LOGI(TAG, "Entering CAL_LC state");
    rgb_led_set(255, 255, 0, LED_PATTERN_PULSE_HALF_HZ);  /* Yellow pulsing */
}

/* CALIBRATE_PRESSURE - Pressure calibration mode */
static void handle_cal_pr_enter(void)
{
    ESP_LOGI(TAG, "Entering CAL_PR state");
    rgb_led_set(255, 255, 0, LED_PATTERN_PULSE_HALF_HZ);  /* Yellow pulsing */
}

/* IGNITION */
static void handle_ignition_enter(void)
{
    ESP_LOGI(TAG, "Entering IGNITION state");
    rgb_led_set(255, 0, 0, LED_PATTERN_BLINK_2HZ);  /* Red blinking */
    buzzer_play(BUZZER_PATTERN_ALARM);  /* Alert: igniting! */

    /* Get settings */
    const settings_t *settings = settings_get();
    float igniter_time = settings ? settings->igniter_on_time : 1.0f;

    ESP_LOGI(TAG, "Firing igniter for %.1f seconds", igniter_time);

    /* Fire igniter: set GPIO high */
    gpio_set_level(PIN_IGNITION, 1);
    gpio_set_level(PIN_LOW_SIDE_POWER, 1);

    /* Wait for igniter burn time */
    int delay_ms = (int)(igniter_time * 1000);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    /* Turn off igniter */
    gpio_set_level(PIN_IGNITION, 0);
    gpio_set_level(PIN_LOW_SIDE_POWER, 0);

    ESP_LOGI(TAG, "Igniter off, transitioning to TESTRUNNING");

    /* Auto-transition to TESTRUNNING */
    uint8_t cmd = CMD_START_RUNNING;
    xQueueSendToFront(state_event_queue, &cmd, 0);
}

/* TESTRUNNING */
static void handle_testrunning_enter(void)
{
    ESP_LOGI(TAG, "Entering TESTRUNNING state");
    rgb_led_set(255, 255, 0, LED_PATTERN_SOLID);  /* Yellow = testing */
    buzzer_stop();

    /* Initialize test tracking */
    s_test_data.test_sample_count = 0;
    s_test_data.end_counter = 0;
    s_test_data.post_burn_counter = 0;
    s_test_data.last_sample_time_us = esp_timer_get_time();
}

static void handle_testrunning_run(void)
{
    safety_watchdog_feed();

    /* Check for samples from ADC queue */
    adc_sample_t sample;
    while (xQueueReceive(adc_sample_queue, &sample, 0) == pdTRUE) {
        /* Log sample to SD card */
        sd_logger_write_sample(&sample);

        /* Update test statistics */
        s_test_data.test_sample_count++;
        s_test_data.last_sample_time_us = sample.timestamp_us;

        /* Calculate baseline from first 0.5 seconds (approx 50 samples at 10Hz) */
        if (s_test_data.test_sample_count <= 50) {
            /* Accumulate for baseline */
            s_test_data.baseline_thrust += sample.loadcell_kg;
            s_test_data.baseline_pressure += sample.pressure_bar;

            if (s_test_data.test_sample_count == 50) {
                /* Calculate average baseline */
                s_test_data.baseline_thrust /= 50.0f;
                s_test_data.baseline_pressure /= 50.0f;
                ESP_LOGI(TAG, "Baseline: thrust=%.2fkg, pressure=%.2fbar",
                         s_test_data.baseline_thrust, s_test_data.baseline_pressure);
            }
        } else {
            /* Track max values */
            if (sample.loadcell_kg > s_test_data.max_thrust) {
                s_test_data.max_thrust = sample.loadcell_kg;
            }
            if (sample.pressure_bar > s_test_data.max_pressure) {
                s_test_data.max_pressure = sample.pressure_bar;
            }

            /* Accumulate for impulse */
            s_test_data.total_impulse += sample.loadcell_kg;

            /* End-of-burn detection: 5% threshold for END_TEST_DELAY seconds */
            const settings_t *settings = settings_get();
            uint8_t end_delay = settings ? settings->end_test_delay : 5;
            uint32_t confirm_samples = end_delay * 10;  /* 10 Hz sample rate */

            float thrust_threshold = s_test_data.baseline_thrust * 0.05f;
            float pressure_threshold = s_test_data.baseline_pressure * 0.05f;

            bool thrust_at_baseline = fabs(sample.loadcell_kg - s_test_data.baseline_thrust) < thrust_threshold;
            bool pressure_at_baseline = fabs(sample.pressure_bar - s_test_data.baseline_pressure) < pressure_threshold;

            if (thrust_at_baseline && pressure_at_baseline) {
                s_test_data.end_counter++;
                ESP_LOGI(TAG, "At baseline: %lu/%lu samples",
                         s_test_data.end_counter, confirm_samples);

                if (s_test_data.end_counter >= confirm_samples) {
                    /* Burn complete - move to post-burn logging */
                    ESP_LOGI(TAG, "Burn complete, starting post-burn logging");
                    s_test_data.post_burn_counter = 0;

                    /* Continue logging for END_TEST_DELAY seconds */
                    uint32_t post_burn_samples = confirm_samples;
                    while (s_test_data.post_burn_counter < post_burn_samples) {
                        if (xQueueReceive(adc_sample_queue, &sample, pdMS_TO_TICKS(100)) == pdTRUE) {
                            sd_logger_write_sample(&sample);
                            s_test_data.post_burn_counter++;
                            s_test_data.test_sample_count++;
                            s_test_data.total_impulse += sample.loadcell_kg;

                            /* Update max values */
                            if (sample.loadcell_kg > s_test_data.max_thrust) {
                                s_test_data.max_thrust = sample.loadcell_kg;
                            }
                            if (sample.pressure_bar > s_test_data.max_pressure) {
                                s_test_data.max_pressure = sample.pressure_bar;
                            }
                        }
                        safety_watchdog_feed();
                    }

                    /* Transition to ENDTEST */
                    uint8_t cmd = CMD_END_TEST;
                    xQueueSendToFront(state_event_queue, &cmd, 0);
                    return;
                }
            } else {
                /* Reset counter if not at baseline */
                s_test_data.end_counter = 0;
            }
        }
    }

    /* Feeding watchdog already done at start */
}

/* ENDTEST */
static void handle_endtest_enter(void)
{
    ESP_LOGI(TAG, "Entering ENDTEST state");
    rgb_led_set(0, 255, 0, LED_PATTERN_SOLID);  /* Green = complete */
    buzzer_play(BUZZER_PATTERN_DOUBLE_BEEP);  /* Success beep */

    /* Calculate test duration */
    uint64_t end_time_us = esp_timer_get_time();
    float duration = (end_time_us - s_test_data.test_start_time_us) / 1000000.0f;

    ESP_LOGI(TAG, "Test duration: %.3f seconds", duration);
    ESP_LOGI(TAG, "Max thrust: %.3f kg, Max pressure: %.3f bar",
             s_test_data.max_thrust, s_test_data.max_pressure);
    ESP_LOGI(TAG, "Total impulse: %.3f kg*s, Samples: %lu",
             s_test_data.total_impulse, s_test_data.test_sample_count);

    /* Write summary to SD card */
    sd_logger_write_summary(duration, s_test_data.max_thrust,
                           s_test_data.total_impulse, s_test_data.max_pressure);

    /* Close CSV file */
    sd_logger_close();

    /* Clear test active flag */
    s_test_data.test_active = false;

    ESP_LOGI(TAG, "Test complete, file closed");

    /* Auto-transition to IDLE after 2 seconds */
    vTaskDelay(pdMS_TO_TICKS(2000));

    uint8_t cmd = CMD_TO_IDLE;
    xQueueSendToFront(state_event_queue, &cmd, 0);
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
    /* Feed watchdog to keep system alive in WELCOME state */
    safety_watchdog_feed();

    /* 100ms per tick (queue timeout in main loop), 20 ticks = 2000ms */
    s_welcome_ticks++;
    if (s_welcome_ticks >= 20) {
        transition_to(STATE_IDLE);
    }
}

static const state_handlers_t s_handlers[STATE_MAX] = {
    [STATE_INIT]                = { handle_init_enter,      handle_generic_run, handler_stub },
    [STATE_IDLE]                = { handle_idle_enter,       handle_idle_run,     handler_stub },
    [STATE_ARMED]               = { handle_armed_enter,      handle_generic_run, handle_armed_exit },
    [STATE_STARTTEST]           = { handle_starttest_enter,  handle_generic_run, handler_stub },
    [STATE_IGNITION]            = { handle_ignition_enter,   handle_generic_run, handler_stub },
    [STATE_TESTRUNNING]         = { handle_testrunning_enter,handle_testrunning_run,handler_stub },
    [STATE_ENDTEST]             = { handle_endtest_enter,    handle_generic_run, handler_stub },
    [STATE_HALT]                = { handle_halt_enter,       handle_halt_run,     handle_halt_exit },
    [STATE_CHECK_IGNITER]       = { handle_chk_ign_enter,    handle_generic_run, handler_stub },
    [STATE_CHECK_BREAKWIRES]    = { handle_chk_brk_enter,    handle_generic_run, handler_stub },
    [STATE_CALIBRATE_LOADCELL]  = { handle_cal_lc_enter,     handle_generic_run, handler_stub },
    [STATE_CALIBRATE_PRESSURE]  = { handle_cal_pr_enter,     handle_generic_run, handler_stub },
    [STATE_WELCOME_SCREEN]      = { handle_welcome_enter,    handle_welcome_run,  handler_stub },
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

        /* When in test mode, continuously drain events to prevent any processing */
        if (s_test_mode) {
            uint8_t drain_event;
            int drained = 0;
            while (xQueueReceive(state_event_queue, &drain_event, 0) == pdTRUE) {
                drained++;
                if (drained <= 10) {  /* Only log first 10 to avoid spam */
                    ESP_LOGD(TAG, "TEST MODE: Draining event 0x%02x", drain_event);
                }
            }
            if (drained > 10) {
                ESP_LOGW(TAG, "TEST MODE: Drained %d events", drained);
            }
            /* Short delay to prevent CPU spin */
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (xQueueReceive(state_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGD(TAG, "Event 0x%02x in state %s",
                     event, s_state_names[s_current_state]);

            /* In test mode, ignore ALL events except CMD_HALT (safety override) */
            if (s_test_mode && event != CMD_HALT) {
                ESP_LOGD(TAG, "TEST MODE: Ignoring event 0x%02x", event);
                safety_watchdog_feed();  /* Keep watchdog alive */
                continue;
            }

            if (event == CMD_HALT) {
                transition_to(STATE_HALT);

            } else if (event == CMD_COMMS_ERROR) {
                /* Comms lost: halt from any non-HALT state
                 * But NOT during test mode - allow testing without interruptions */
                if (s_current_state != STATE_HALT && !s_test_mode) {
                    ESP_LOGE(TAG, "COMMS_ERROR: transitioning to HALT");
                    transition_to(STATE_HALT);
                } else if (s_test_mode) {
                    ESP_LOGW(TAG, "COMMS_ERROR ignored (test mode active)");
                    /* Feed watchdog when in test mode to prevent timeout */
                    safety_watchdog_feed();
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

            } else if (event == CMD_BTN_SHORT_PRESS) {
                switch (s_current_state) {
                    case STATE_IDLE:
                        /* Short press in IDLE while SAFE goes to igniter check */
                        transition_to(STATE_CHECK_IGNITER);
                        break;
                    default:
                        /* Short press has no effect in other states */
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

            } else if (event == CMD_IGNITION) {
                /* Internal: transition to IGNITION state */
                if (s_current_state == STATE_STARTTEST) {
                    transition_to(STATE_IGNITION);
                }

            } else if (event == CMD_START_RUNNING) {
                /* Internal: transition to TESTRUNNING state */
                if (s_current_state == STATE_IGNITION) {
                    transition_to(STATE_TESTRUNNING);
                }

            } else if (event == CMD_END_TEST) {
                /* Internal: transition to ENDTEST state */
                if (s_current_state == STATE_TESTRUNNING) {
                    transition_to(STATE_ENDTEST);
                }

            } else if (event == CMD_TO_IDLE) {
                /* Internal: transition to IDLE state */
                if (s_current_state == STATE_ENDTEST) {
                    transition_to(STATE_IDLE);
                }
            }
        }
    }
}

/*******************************************************************************
 * Public API - Test Interface
 ******************************************************************************/

/* Map state name string to enum */
base_state_t state_machine_from_name(const char *name)
{
    if (!name) return STATE_MAX;

    for (int i = 0; i < STATE_MAX; i++) {
        if (strcmp(s_state_names[i], name) == 0) {
            return (base_state_t)i;
        }
    }
    return STATE_MAX;
}

/* Force state transition for testing (bypasses normal validation) */
esp_err_t state_machine_force_state(base_state_t new_state)
{
    if (new_state >= STATE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Drain ALL events from state_event_queue to prevent them from
       immediately changing the state after the forced transition.
       In TEST MODE, we want complete control over state transitions. */
    uint8_t stale_event;
    int drained_count = 0;
    while (xQueueReceive(state_event_queue, &stale_event, 0) == pdTRUE) {
        drained_count++;
        ESP_LOGD(TAG, "TEST MODE: Drained event 0x%02x", stale_event);
    }
    if (drained_count > 0) {
        ESP_LOGW(TAG, "TEST MODE: Drained %d events from state_event_queue", drained_count);
    }

    base_state_t old_state = s_current_state;

    ESP_LOGW(TAG, "TEST MODE: Forcing transition %s -> %s (bypassing validation)",
             s_state_names[old_state], s_state_names[new_state]);

    /* Call exit handler for old state */
    if (s_handlers[old_state].on_exit) {
        s_handlers[old_state].on_exit();
    }

    /* Set new state */
    s_current_state = new_state;

    /* Call enter handler for new state */
    if (s_handlers[new_state].on_enter) {
        s_handlers[new_state].on_enter();
    }

    /* Broadcast state change */
    broadcast_state_change(new_state);

    /* Allow time for ESP-NOW packet to be sent and processed by REMOTE
     * Drain events continuously during the delay to prevent any accumulation */
    for (int i = 0; i < 20; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        while (xQueueReceive(state_event_queue, &stale_event, 0) == pdTRUE) {
            drained_count++;
            ESP_LOGD(TAG, "TEST MODE: Drained event during delay 0x%02x", stale_event);
        }
    }

    if (drained_count > 0) {
        ESP_LOGW(TAG, "TEST MODE: Drained %d total events", drained_count);
    }

    /* Feed watchdog to prevent it from triggering during test */
    safety_watchdog_feed();

    return ESP_OK;
}

/* Set test mode flag to enable/disable automatic HALT transitions */
void state_machine_set_test_mode(bool enabled)
{
    s_test_mode = enabled;
    ESP_LOGW(TAG, "TEST MODE: %s", enabled ? "ENABLED" : "DISABLED");
}

#endif /* BUILD_TARGET_BASE */
