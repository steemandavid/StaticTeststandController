/*******************************************************************************
 * Common - WS2812 RGB LED Controller
 *
 * Controls RGB LEDs with various patterns (breathing, blink, pulse).
 * See FSD Section 5.3 for state-color mappings.
 ******************************************************************************/

#include "config.h"
#include "rgb_led.h"

#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>

static const char *TAG = "rgb_led";

#define LED_TASK_INTERVAL_MS  20

static led_strip_handle_t s_led_strip = NULL;
static SemaphoreHandle_t s_led_mutex = NULL;

static uint8_t s_target_r = 0;
static uint8_t s_target_g = 0;
static uint8_t s_target_b = 0;
static led_pattern_t s_pattern = LED_PATTERN_OFF;

esp_err_t rgb_led_init(void)
{
    ESP_LOGI(TAG, "Initializing RGB LED on GPIO %d", PIN_RGB_LED);

    s_led_mutex = xSemaphoreCreateMutex();
    if (s_led_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED mutex");
        return ESP_FAIL;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_RGB_LED,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,  /* 10 MHz */
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    led_strip_clear(s_led_strip);
    ESP_LOGI(TAG, "RGB LED initialized");
    return ESP_OK;
}

esp_err_t rgb_led_set(uint8_t r, uint8_t g, uint8_t b, led_pattern_t pattern)
{
    if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_target_r = r;
        s_target_g = g;
        s_target_b = b;
        s_pattern = pattern;
        xSemaphoreGive(s_led_mutex);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void set_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_led_strip != NULL) {
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
    }
}

void rgb_led_task(void *pvParameters)
{
    ESP_LOGI(TAG, "RGB LED task started");

    uint32_t tick = 0;

    for (;;) {
        uint8_t r, g, b;
        led_pattern_t pat;

        if (xSemaphoreTake(s_led_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            r = s_target_r;
            g = s_target_g;
            b = s_target_b;
            pat = s_pattern;
            xSemaphoreGive(s_led_mutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(LED_TASK_INTERVAL_MS));
            continue;
        }

        float brightness = 1.0f;

        switch (pat) {
            case LED_PATTERN_OFF:
                brightness = 0.0f;
                break;

            case LED_PATTERN_SOLID:
                brightness = 1.0f;
                break;

            case LED_PATTERN_BREATHING: {
                /* Sinusoidal fade: period = 2s = 100 ticks at 20ms */
                float phase = (float)(tick % 100) / 100.0f;
                brightness = (sinf(phase * 2.0f * M_PI) + 1.0f) / 2.0f;
                break;
            }

            case LED_PATTERN_BLINK_2HZ:
                /* 250ms on, 250ms off -> period 25 ticks */
                brightness = ((tick % 25) < 13) ? 1.0f : 0.0f;
                break;

            case LED_PATTERN_PULSE_HALF_HZ: {
                /* Slow pulse: period = 2s = 100 ticks */
                float phase = (float)(tick % 100) / 100.0f;
                brightness = (phase < 0.5f) ?
                    (sinf(phase * 2.0f * M_PI) + 1.0f) / 2.0f : 0.0f;
                break;
            }

            case LED_PATTERN_RAPID_BLINK_5HZ:
                /* 100ms on, 100ms off -> period 10 ticks */
                brightness = ((tick % 10) < 5) ? 1.0f : 0.0f;
                break;
        }

        set_pixel((uint8_t)(r * brightness),
                  (uint8_t)(g * brightness),
                  (uint8_t)(b * brightness));

        tick++;
        vTaskDelay(pdMS_TO_TICKS(LED_TASK_INTERVAL_MS));
    }
}
