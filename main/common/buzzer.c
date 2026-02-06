/*******************************************************************************
 * Common - Buzzer Controller
 *
 * Pattern-based buzzer driver for both BASE and REMOTE units.
 * Uses a FreeRTOS queue to receive pattern requests and drives
 * PIN_BUZZER low/high with appropriate timing (active-low buzzers).
 ******************************************************************************/

#include "config.h"
#include "buzzer.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "buzzer";

static QueueHandle_t s_buzzer_queue = NULL;

esp_err_t buzzer_init(void)
{
    /* Depth 1: only the most recent pattern matters */
    s_buzzer_queue = xQueueCreate(1, sizeof(buzzer_pattern_t));
    if (s_buzzer_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create buzzer queue");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Buzzer initialized");
    return ESP_OK;
}

void buzzer_play(buzzer_pattern_t pattern)
{
    if (s_buzzer_queue != NULL) {
        xQueueOverwrite(s_buzzer_queue, &pattern);
    }
}

void buzzer_stop(void)
{
    buzzer_play(BUZZER_PATTERN_OFF);
}

static void buzzer_on(void)
{
    gpio_set_level(PIN_BUZZER, 0);  /* Active low */
}

static void buzzer_off(void)
{
    gpio_set_level(PIN_BUZZER, 1);  /* Active low */
}

void buzzer_task(void *pvParameters)
{
    buzzer_pattern_t pattern;
    bool alarm_active = false;
    bool alarm_phase = false;  /* false = on phase, true = off phase */

    ESP_LOGI(TAG, "Buzzer task started");

    for (;;) {
        TickType_t wait = alarm_active ? pdMS_TO_TICKS(50) : portMAX_DELAY;

        if (xQueueReceive(s_buzzer_queue, &pattern, wait) == pdTRUE) {
            /* New pattern received */
            buzzer_off();
            alarm_active = false;

            switch (pattern) {
                case BUZZER_PATTERN_OFF:
                    break;

                case BUZZER_PATTERN_SHORT_BEEP:
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_off();
                    break;

                case BUZZER_PATTERN_LONG_BEEP:
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    buzzer_off();
                    break;

                case BUZZER_PATTERN_DOUBLE_BEEP:
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_off();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    buzzer_off();
                    break;

                case BUZZER_PATTERN_ALARM:
                    alarm_active = true;
                    alarm_phase = false;
                    buzzer_on();
                    vTaskDelay(pdMS_TO_TICKS(250));
                    break;
            }
        } else if (alarm_active) {
            /* Timeout during alarm — toggle */
            alarm_phase = !alarm_phase;
            if (alarm_phase) {
                buzzer_off();
            } else {
                buzzer_on();
            }
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
}
