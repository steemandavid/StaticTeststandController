/*******************************************************************************
 * REMOTE - Command Dispatch
 *
 * Consumes LED/buzzer/button-LED commands from input_event_queue.
 * Commands >= 0x40 are BASE-to-REMOTE output commands routed here
 * by espnow_rx_task.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_REMOTE

#include "cmd_dispatch.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "rgb_led.h"
#include "buzzer.h"
#include "button_led.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "cmd_dispatch";

void cmd_dispatch_task(void *pvParameters)
{
    uint8_t cmd;
    ESP_LOGI(TAG, "Command dispatch task started");

    for (;;) {
        if (xQueueReceive(input_event_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* Only handle BASE-to-REMOTE output commands (0x40+) */
        if (cmd < 0x40) {
            continue;
        }

        ESP_LOGD(TAG, "Dispatching cmd=0x%02x", cmd);

        switch (cmd) {
            case CMD_LED_SAFE:
                rgb_led_set(0, 255, 0, LED_PATTERN_BREATHING);
                break;

            case CMD_LED_ARMED:
                rgb_led_set(255, 165, 0, LED_PATTERN_SOLID);
                break;

            case CMD_LED_TESTING:
                rgb_led_set(255, 165, 0, LED_PATTERN_BLINK_2HZ);
                break;

            case CMD_LED_COMPLETE:
                rgb_led_set(0, 255, 0, LED_PATTERN_SOLID);
                break;

            case CMD_LED_ERROR:
                rgb_led_set(255, 0, 0, LED_PATTERN_PULSE_HALF_HZ);
                break;

            case CMD_LED_SWITCH_ERR:
                rgb_led_set(255, 0, 0, LED_PATTERN_RAPID_BLINK_5HZ);
                break;

            case CMD_BUZZER_ON:
                buzzer_play(BUZZER_PATTERN_SHORT_BEEP);
                break;

            case CMD_BUZZER_OFF:
                buzzer_stop();
                break;

            case CMD_BUTTON_LED_ON:
                button_led_set(BUTTON_LED_SOLID);
                break;

            case CMD_BUTTON_LED_OFF:
                button_led_set(BUTTON_LED_OFF);
                break;

            default:
                ESP_LOGW(TAG, "Unknown command: 0x%02x", cmd);
                break;
        }
    }
}

#endif /* BUILD_TARGET_REMOTE */
