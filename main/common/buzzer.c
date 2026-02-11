/*******************************************************************************
 * Common - Buzzer Controller
 *
 * Pattern-based buzzer driver for both BASE and REMOTE units.
 * Uses a FreeRTOS queue to receive pattern requests and drives
 * PIN_BUZZER with PWM tone generation (active-LOW buzzer).
 *
 * Hardware: Buzzer positive connected to VCC, negative to GPIO.
 * OFF = GPIO HIGH, ON = GPIO LOW (PWM toggles).
 ******************************************************************************/

#include "config.h"
#include "buzzer.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "buzzer";

static QueueHandle_t s_buzzer_queue = NULL;

/* Forward declarations */
static void buzzer_off(void);
static void buzzer_on(void);

/* LEDC PWM configuration for tone generation */
#define BUZZER_LEDC_TIMER       LEDC_TIMER_0
#define BUZZER_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BUZZER_LEDC_DUTY_RES    5000  /* 50% duty cycle */
#define BUZZER_FREQ_HZ         4000  /* PWM frequency */

esp_err_t buzzer_init(void)
{
    /* First, ensure GPIO is in OFF state before configuring PWM */
    gpio_config_t buzzer_gpio = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&buzzer_gpio);

    /* Set GPIO HIGH first (OFF for active-low buzzer) */
    gpio_set_level(PIN_BUZZER, 1);

    /* Depth 1: only the most recent pattern matters */
    s_buzzer_queue = xQueueCreate(1, sizeof(buzzer_pattern_t));
    if (s_buzzer_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create buzzer queue");
        return ESP_FAIL;
    }

    /* Initialize queue with OFF pattern to ensure buzzer starts silent */
    buzzer_pattern_t initial_pattern = BUZZER_PATTERN_OFF;
    xQueueSend(s_buzzer_queue, &initial_pattern, 0);

    /* Configure LEDC PWM for tone generation */
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT, /* 5000 = 50% duty @ 13-bit */
        .timer_num = BUZZER_LEDC_TIMER,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = BUZZER_LEDC_MODE,
        .channel = BUZZER_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = 1,  /* Invert for active-low buzzer (VCC to buzzer+, GPIO to buzzer-) */
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));

    /* Ensure buzzer is OFF - stop PWM and set GPIO HIGH */
    buzzer_off();

    ESP_LOGI(TAG, "Buzzer initialized (active-LOW PWM tone mode)");
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
    /* Enable tone output at 4000Hz with 50% duty cycle */
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, BUZZER_LEDC_DUTY_RES);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
}

static void buzzer_off(void)
{
    /* Turn off buzzer aggressively for active-low hardware */

    /* 1. Stop the PWM signal completely */
    ledc_stop(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 1);  /* 1 = output HIGH after stop */

    /* 2. Set duty cycle to 0 to ensure no signal */
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);

    /* 3. Force GPIO HIGH (OFF for active-low: VCC→buzzer+, GPIO HIGH→buzzer-) */
    gpio_set_level(PIN_BUZZER, 1);
}

void buzzer_task(void *pvParameters)
{
    buzzer_pattern_t pattern;
    bool alarm_active = false;
    bool alarm_phase = false;  /* false = on phase, true = off phase */

    ESP_LOGI(TAG, "Buzzer task started");

    /* Ensure buzzer is OFF when task starts */
    buzzer_off();

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
