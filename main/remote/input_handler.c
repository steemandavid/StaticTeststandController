/*******************************************************************************
 * REMOTE Unit - Input Handler
 *
 * Handles button presses and switch states with debouncing.
 * See FSD Section 5.2 for input detection specifications.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_REMOTE

#include "input_handler.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "input";

#define POLL_INTERVAL_MS  10

typedef enum {
    BTN_IDLE,
    BTN_PRESSED,
    BTN_WAIT_RELEASE,
    BTN_WAIT_DOUBLE
} button_fsm_t;

static button_fsm_t s_btn_state = BTN_IDLE;
static int64_t s_btn_press_time = 0;
static int64_t s_btn_release_time = 0;

typedef enum {
    SW_UNKNOWN,
    SW_SAFE,
    SW_ARMED,
    SW_ERROR
} switch_state_t;

static switch_state_t s_switch_state = SW_UNKNOWN;
static int s_switch_debounce_count = 0;
static switch_state_t s_switch_pending = SW_UNKNOWN;

#define DEBOUNCE_SAMPLES (DEBOUNCE_MS / POLL_INTERVAL_MS)

esp_err_t input_handler_init(void)
{
    ESP_LOGI(TAG, "Initializing input handler");

    /* Button - active low with pull-up */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    /* Switches - active low with pull-ups */
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << PIN_SWITCH_ARMED) | (1ULL << PIN_SWITCH_SAFE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&sw_cfg);

    ESP_LOGI(TAG, "Input handler initialized");
    return ESP_OK;
}

static void send_input_event(input_event_t evt)
{
    uint8_t evt_byte = (uint8_t)evt;
    xQueueSend(input_event_queue, &evt_byte, pdMS_TO_TICKS(50));

    /* Map input event to ESP-NOW command and forward to BASE */
    espnow_packet_t pkt = {0};
    switch (evt) {
        case INPUT_BUTTON_SHORT_PRESS:  pkt.command = CMD_BTN_SHORT_PRESS;  break;
        case INPUT_BUTTON_LONG_PRESS:   pkt.command = CMD_BTN_LONG_PRESS;   break;
        case INPUT_BUTTON_DOUBLE_PRESS: pkt.command = CMD_BTN_DOUBLE_PRESS; break;
        case INPUT_SWITCH_TO_ARMED:     pkt.command = CMD_SWITCH_TO_ARMED;  break;
        case INPUT_SWITCH_TO_SAFE:      pkt.command = CMD_SWITCH_TO_SAFE;   break;
        case INPUT_SWITCH_ERROR:        pkt.command = CMD_COMMS_ERROR;      break;
    }
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Input event: %d -> cmd 0x%02x", evt, pkt.command);
}

static void process_button(bool pressed, int64_t now_us)
{
    switch (s_btn_state) {
        case BTN_IDLE:
            if (pressed) {
                s_btn_press_time = now_us;
                s_btn_state = BTN_PRESSED;
            }
            break;

        case BTN_PRESSED:
            if (!pressed) {
                /* Released */
                int64_t held_ms = (now_us - s_btn_press_time) / 1000;
                if (held_ms >= LONG_PRESS_MS) {
                    send_input_event(INPUT_BUTTON_LONG_PRESS);
                    s_btn_state = BTN_IDLE;
                } else {
                    /* Short press - wait for possible double press */
                    s_btn_release_time = now_us;
                    s_btn_state = BTN_WAIT_DOUBLE;
                }
            } else {
                /* Still held - check for long press */
                int64_t held_ms = (now_us - s_btn_press_time) / 1000;
                if (held_ms >= LONG_PRESS_MS) {
                    send_input_event(INPUT_BUTTON_LONG_PRESS);
                    s_btn_state = BTN_WAIT_RELEASE;
                }
            }
            break;

        case BTN_WAIT_RELEASE:
            if (!pressed) {
                s_btn_state = BTN_IDLE;
            }
            break;

        case BTN_WAIT_DOUBLE:
            if (pressed) {
                /* Second press within window = double press */
                send_input_event(INPUT_BUTTON_DOUBLE_PRESS);
                s_btn_state = BTN_WAIT_RELEASE;
            } else {
                int64_t wait_ms = (now_us - s_btn_release_time) / 1000;
                if (wait_ms >= DOUBLE_PRESS_MS) {
                    /* Timeout - it was a single short press */
                    send_input_event(INPUT_BUTTON_SHORT_PRESS);
                    s_btn_state = BTN_IDLE;
                }
            }
            break;
    }
}

static switch_state_t read_switch_raw(void)
{
    int armed = !gpio_get_level(PIN_SWITCH_ARMED);  /* active low */
    int safe  = !gpio_get_level(PIN_SWITCH_SAFE);   /* active low */

    if (armed && !safe) {
        return SW_ARMED;
    } else if (!armed && safe) {
        return SW_SAFE;
    } else {
        return SW_ERROR;
    }
}

static void process_switch(void)
{
    switch_state_t raw = read_switch_raw();

    if (raw == s_switch_pending) {
        s_switch_debounce_count++;
    } else {
        s_switch_pending = raw;
        s_switch_debounce_count = 0;
    }

    if (s_switch_debounce_count >= DEBOUNCE_SAMPLES && raw != s_switch_state) {
        s_switch_state = raw;
        s_switch_debounce_count = 0;

        switch (raw) {
            case SW_ARMED:
                send_input_event(INPUT_SWITCH_TO_ARMED);
                break;
            case SW_SAFE:
                send_input_event(INPUT_SWITCH_TO_SAFE);
                break;
            case SW_ERROR:
                send_input_event(INPUT_SWITCH_ERROR);
                break;
            default:
                break;
        }
    }
}

void input_handler_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Input handler task started");

    for (;;) {
        int64_t now_us = esp_timer_get_time();
        bool btn_pressed = !gpio_get_level(PIN_BUTTON);  /* active low */

        process_button(btn_pressed, now_us);
        process_switch();

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

#endif /* BUILD_TARGET_REMOTE */
