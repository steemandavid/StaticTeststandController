/*******************************************************************************
 * Common - Serial Test Protocol
 *
 * Provides a serial command interface for automated hardware testing.
 * Listens on the USB serial console for "TEST " prefixed commands and
 * returns JSON-formatted responses for machine parsing.
 ******************************************************************************/

#include "config.h"
#include "version.h"
#include "test_protocol.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "buzzer.h"
#include "rgb_led.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef BUILD_TARGET_BASE
#include "state_machine.h"
#endif

#ifdef BUILD_TARGET_REMOTE
#include "display_ssd1306.h"
#endif

static const char *TAG = "test_proto";

#define TEST_UART_NUM       UART_NUM_0
#define TEST_BUF_SIZE       256
#define TEST_PREFIX         "TEST "
#define TEST_PREFIX_LEN     5

static volatile bool s_active = false;

/*******************************************************************************
 * Response Helpers
 ******************************************************************************/

static void send_ok(const char *command, const char *data_json)
{
    printf("{\"status\":\"ok\",\"command\":\"%s\",\"data\":%s}\n", command, data_json);
    fflush(stdout);
}

static void send_error(const char *command, const char *message)
{
    printf("{\"status\":\"error\",\"command\":\"%s\",\"message\":\"%s\"}\n",
           command, message);
    fflush(stdout);
}

/*******************************************************************************
 * Command Handlers
 ******************************************************************************/

static void handle_ping(void)
{
    send_ok("PING", "{\"pong\":true}");
}

static void handle_info(void)
{
    int64_t uptime_us = esp_timer_get_time();
    int64_t uptime_s = uptime_us / 1000000;
    char buf[256];

#ifdef BUILD_TARGET_BASE
    snprintf(buf, sizeof(buf),
             "{\"target\":\"BASE\",\"project\":\"StaticTeststandController\","
             "\"version\":\"%s\",\"idf_version\":\"%s\",\"uptime_s\":%lld,"
             "\"chip\":\"ESP32-S3\"}",
             VERSION_STRING, esp_get_idf_version(), (long long)uptime_s);
#else
    snprintf(buf, sizeof(buf),
             "{\"target\":\"REMOTE\",\"project\":\"StaticTeststandController\","
             "\"version\":\"%s\",\"idf_version\":\"%s\",\"uptime_s\":%lld,"
             "\"chip\":\"ESP32-S3\"}",
             VERSION_STRING, esp_get_idf_version(), (long long)uptime_s);
#endif
    send_ok("INFO", buf);
}

static void handle_state(const char *args)
{
#ifdef BUILD_TARGET_BASE
    /* If args provided, try to set state (testing only) */
    if (args && strlen(args) > 0) {
        base_state_t new_state = state_machine_from_name(args);
        if (new_state >= STATE_MAX) {
            send_error("STATE", "Invalid state name");
            return;
        }

        esp_err_t ret = state_machine_force_state(new_state);
        if (ret == ESP_OK) {
            char buf[96];
            snprintf(buf, sizeof(buf), "{\"state\":%d,\"name\":\"%s\",\"set\":true}",
                     (int)new_state, state_machine_get_name(new_state));
            send_ok("STATE", buf);
        } else {
            send_error("STATE", "Failed to set state");
        }
        return;
    }

    /* No args: read current state */
    base_state_t state = state_machine_get_state();
    const char *name = state_machine_get_name(state);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"state\":%d,\"name\":\"%s\"}", (int)state, name);
    send_ok("STATE", buf);
#else
    send_error("STATE", "STATE command only available on BASE unit");
#endif
}

static void handle_base_state(void)
{
#ifdef BUILD_TARGET_REMOTE
    /* BASE_STATE query returns the current BASE state displayed on REMOTE */
    const char *base_state = display_get_base_state();
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"base_state\":\"%s\"}", base_state);
    send_ok("BASE_STATE", buf);
#else
    send_error("BASE_STATE", "BASE_STATE command only available on REMOTE unit");
#endif
}

static void handle_test_mode(const char *args)
{
#ifdef BUILD_TARGET_BASE
    /* TEST_MODE ON/OFF - enable or disable test mode */
    if (strcmp(args, "ON") == 0) {
        state_machine_set_test_mode(true);
        send_ok("TEST_MODE", "{\"enabled\":true}");
    } else if (strcmp(args, "OFF") == 0) {
        state_machine_set_test_mode(false);
        send_ok("TEST_MODE", "{\"enabled\":false}");
    } else {
        send_error("TEST_MODE", "Usage: TEST TEST_MODE ON|OFF");
    }
#else
    send_error("TEST_MODE", "TEST_MODE command only available on BASE unit");
#endif
}

static void handle_gpio_read(const char *args)
{
    int pin = atoi(args);
    if (pin < 0 || pin > 48) {
        send_error("GPIO READ", "Invalid pin number");
        return;
    }
    int level = gpio_get_level(pin);
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"pin\":%d,\"level\":%d}", pin, level);
    send_ok("GPIO READ", buf);
}

static void handle_gpio_write(const char *args)
{
    int pin = 0, level = 0;
    if (sscanf(args, "%d %d", &pin, &level) != 2) {
        send_error("GPIO WRITE", "Usage: TEST GPIO WRITE <pin> <level>");
        return;
    }
    if (pin < 0 || pin > 48) {
        send_error("GPIO WRITE", "Invalid pin number");
        return;
    }
    if (level != 0 && level != 1) {
        send_error("GPIO WRITE", "Level must be 0 or 1");
        return;
    }
    gpio_set_level(pin, level);
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"pin\":%d,\"level\":%d}", pin, level);
    send_ok("GPIO WRITE", buf);
}

static void handle_queue_status(void)
{
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"espnow_rx\":%d,\"espnow_tx\":%d,"
        "\"input_event\":%d,\"display_cmd\":%d,\"state_event\":%d",
        (int)uxQueueMessagesWaiting(espnow_rx_queue),
        (int)uxQueueMessagesWaiting(espnow_tx_queue),
        (int)uxQueueMessagesWaiting(input_event_queue),
        (int)uxQueueMessagesWaiting(display_cmd_queue),
        (int)uxQueueMessagesWaiting(state_event_queue));

#ifdef BUILD_TARGET_BASE
    n += snprintf(buf + n, sizeof(buf) - n,
        ",\"adc_sample\":%d,\"log\":%d",
        (int)uxQueueMessagesWaiting(adc_sample_queue),
        (int)uxQueueMessagesWaiting(log_queue));
#endif
    snprintf(buf + n, sizeof(buf) - n, "}");
    send_ok("QUEUE STATUS", buf);
}

static void handle_heap(void)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"free_heap\":%lu,\"min_free_heap\":%lu,\"total_heap\":%lu}",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)esp_get_minimum_free_heap_size(),
             (unsigned long)heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
    send_ok("HEAP", buf);
}

static void handle_tasks(void)
{
    UBaseType_t count = uxTaskGetNumberOfTasks();
    TaskStatus_t *task_array = malloc(count * sizeof(TaskStatus_t));
    if (task_array == NULL) {
        send_error("TASKS", "Failed to allocate memory");
        return;
    }

    UBaseType_t actual = uxTaskGetSystemState(task_array, count, NULL);

    /* Build JSON array */
    printf("{\"status\":\"ok\",\"command\":\"TASKS\",\"data\":{\"count\":%lu,\"tasks\":[",
           (unsigned long)actual);
    for (UBaseType_t i = 0; i < actual; i++) {
        if (i > 0) printf(",");
        printf("{\"name\":\"%s\",\"state\":%d,\"priority\":%lu,\"stack_hwm\":%lu}",
               task_array[i].pcTaskName,
               (int)task_array[i].eCurrentState,
               (unsigned long)task_array[i].uxCurrentPriority,
               (unsigned long)task_array[i].usStackHighWaterMark);
    }
    printf("]}}\n");
    fflush(stdout);

    free(task_array);
}

static void handle_espnow_status(void)
{
    /* Report basic ESP-NOW status */
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"initialized\":true}");
    send_ok("ESPNOW STATUS", buf);
}

static void handle_buzzer(const char *args)
{
    if (strcmp(args, "OFF") == 0) {
        buzzer_stop();
        send_ok("BUZZER", "{\"pattern\":\"off\"}");
    } else if (strcmp(args, "SHORT") == 0) {
        buzzer_play(BUZZER_PATTERN_SHORT_BEEP);
        send_ok("BUZZER", "{\"pattern\":\"short\"}");
    } else if (strcmp(args, "LONG") == 0) {
        buzzer_play(BUZZER_PATTERN_LONG_BEEP);
        send_ok("BUZZER", "{\"pattern\":\"long\"}");
    } else if (strcmp(args, "DOUBLE") == 0) {
        buzzer_play(BUZZER_PATTERN_DOUBLE_BEEP);
        send_ok("BUZZER", "{\"pattern\":\"double\"}");
    } else if (strcmp(args, "ALARM") == 0) {
        buzzer_play(BUZZER_PATTERN_ALARM);
        send_ok("BUZZER", "{\"pattern\":\"alarm\"}");
    } else {
        send_error("BUZZER", "Usage: TEST BUZZER OFF|SHORT|LONG|DOUBLE|ALARM");
    }
}

static void handle_led(const char *args)
{
    if (strcmp(args, "STATE") == 0) {
#ifdef BUILD_TARGET_BASE
        /* Restore LED to current state color */
        base_state_t state = state_machine_get_state();
        switch (state) {
            case STATE_IDLE:
                rgb_led_set(0, 255, 0, LED_PATTERN_BREATHING);
                break;
            case STATE_ARMED:
                rgb_led_set(255, 165, 0, LED_PATTERN_SOLID);
                break;
            case STATE_HALT:
                rgb_led_set(255, 0, 0, LED_PATTERN_PULSE_HALF_HZ);
                break;
            case STATE_WELCOME_SCREEN:
                rgb_led_set(0, 0, 255, LED_PATTERN_BREATHING);
                break;
            default:
                rgb_led_set(0, 255, 0, LED_PATTERN_BREATHING);
                break;
        }
        send_ok("LED", "{\"restored\":\"state\"}");
#else
        send_error("LED", "LED STATE only available on BASE unit");
#endif
        return;
    }

    if (strcmp(args, "OFF") == 0) {
        rgb_led_set(0, 0, 0, LED_PATTERN_OFF);
        send_ok("LED", "{\"r\":0,\"g\":0,\"b\":0,\"pattern\":\"off\"}");
        return;
    }

    /* Parse: LED SET <r> <g> <b> [pattern] */
    if (strncmp(args, "SET ", 4) != 0) {
        send_error("LED", "Usage: TEST LED OFF or TEST LED SET <r> <g> <b> [pattern]");
        return;
    }

    int r = 0, g = 0, b = 0;
    char pattern_str[16] = "solid";
    int parsed = sscanf(args + 4, "%d %d %d %15s", &r, &g, &b, pattern_str);
    if (parsed < 3) {
        send_error("LED", "Usage: TEST LED SET <r> <g> <b> [pattern]");
        return;
    }

    /* Clamp values */
    if (r < 0) { r = 0; }
    if (r > 255) { r = 255; }
    if (g < 0) { g = 0; }
    if (g > 255) { g = 255; }
    if (b < 0) { b = 0; }
    if (b > 255) { b = 255; }

    led_pattern_t pattern = LED_PATTERN_SOLID;
    if (strcmp(pattern_str, "off") == 0) {
        pattern = LED_PATTERN_OFF;
    } else if (strcmp(pattern_str, "solid") == 0) {
        pattern = LED_PATTERN_SOLID;
    } else if (strcmp(pattern_str, "breathing") == 0) {
        pattern = LED_PATTERN_BREATHING;
    } else if (strcmp(pattern_str, "blink") == 0) {
        pattern = LED_PATTERN_BLINK_2HZ;
    } else if (strcmp(pattern_str, "pulse") == 0) {
        pattern = LED_PATTERN_PULSE_HALF_HZ;
    } else if (strcmp(pattern_str, "rapid") == 0) {
        pattern = LED_PATTERN_RAPID_BLINK_5HZ;
    }

    rgb_led_set((uint8_t)r, (uint8_t)g, (uint8_t)b, pattern);

    char buf[96];
    snprintf(buf, sizeof(buf), "{\"r\":%d,\"g\":%d,\"b\":%d,\"pattern\":\"%s\"}",
             r, g, b, pattern_str);
    send_ok("LED", buf);
}

static void handle_reset(void)
{
    send_ok("RESET", "{\"resetting\":true}");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

/*******************************************************************************
 * Command Dispatcher
 ******************************************************************************/

static void dispatch_command(char *cmd_line)
{
    /* Trim trailing whitespace/newline */
    size_t len = strlen(cmd_line);
    while (len > 0 && (cmd_line[len - 1] == '\n' || cmd_line[len - 1] == '\r' ||
                        cmd_line[len - 1] == ' ')) {
        cmd_line[--len] = '\0';
    }

    ESP_LOGD(TAG, "Command: '%s'", cmd_line);

    if (strcmp(cmd_line, "PING") == 0) {
        handle_ping();
    } else if (strcmp(cmd_line, "INFO") == 0) {
        handle_info();
    } else if (strcmp(cmd_line, "STATE") == 0) {
        handle_state(NULL);  /* Read current state */
    } else if (strncmp(cmd_line, "STATE ", 6) == 0) {
        handle_state(cmd_line + 6);  /* Set state */
    } else if (strncmp(cmd_line, "GPIO READ ", 10) == 0) {
        handle_gpio_read(cmd_line + 10);
    } else if (strncmp(cmd_line, "GPIO WRITE ", 11) == 0) {
        handle_gpio_write(cmd_line + 11);
    } else if (strcmp(cmd_line, "QUEUE STATUS") == 0) {
        handle_queue_status();
    } else if (strcmp(cmd_line, "HEAP") == 0) {
        handle_heap();
    } else if (strcmp(cmd_line, "TASKS") == 0) {
        handle_tasks();
    } else if (strcmp(cmd_line, "ESPNOW STATUS") == 0) {
        handle_espnow_status();
    } else if (strncmp(cmd_line, "BUZZER ", 7) == 0) {
        handle_buzzer(cmd_line + 7);
    } else if (strncmp(cmd_line, "LED ", 4) == 0) {
        handle_led(cmd_line + 4);
    } else if (strcmp(cmd_line, "BASE_STATE") == 0) {
        handle_base_state();
    } else if (strncmp(cmd_line, "TEST_MODE ", 10) == 0) {
        handle_test_mode(cmd_line + 10);
    } else if (strcmp(cmd_line, "RESET") == 0) {
        handle_reset();
    } else {
        send_error("UNKNOWN", cmd_line);
    }
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

esp_err_t test_protocol_init(void)
{
    ESP_LOGI(TAG, "Test protocol initialized");
    return ESP_OK;
}

bool test_protocol_is_active(void)
{
    return s_active;
}

void test_protocol_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Test protocol task started");
    s_active = true;

    char line_buf[TEST_BUF_SIZE];
    int line_pos = 0;

    for (;;) {
        /* Read one byte at a time from stdin (USB serial) */
        int c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (c == '\n' || c == '\r') {
            if (line_pos == 0) continue;  /* Skip empty lines */

            line_buf[line_pos] = '\0';

            /* Check for TEST prefix */
            if (strncmp(line_buf, TEST_PREFIX, TEST_PREFIX_LEN) == 0) {
                dispatch_command(line_buf + TEST_PREFIX_LEN);
            }

            line_pos = 0;
        } else if (line_pos < TEST_BUF_SIZE - 1) {
            line_buf[line_pos++] = (char)c;
        }
    }
}
