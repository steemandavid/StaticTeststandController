/*******************************************************************************
 * REMOTE - Button LED Controller
 *
 * Controls the illuminated pushbutton LED on PIN_LED_BUTTON.
 * Supports off, solid on, and 2Hz blink modes via esp_timer.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_REMOTE

#include "button_led.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "button_led";

static esp_timer_handle_t s_blink_timer = NULL;
static volatile bool s_led_state = false;

static void blink_timer_cb(void *arg)
{
    s_led_state = !s_led_state;
    gpio_set_level(PIN_LED_BUTTON, s_led_state ? 0 : 1);  /* active-LOW: ON=0, OFF=1 */
}

esp_err_t button_led_init(void)
{
    /* Configure button LED GPIO as output */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << PIN_LED_BUTTON),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);

    /* Start with LED OFF (hardware is active-LOW, so OFF = GPIO HIGH) */
    gpio_set_level(PIN_LED_BUTTON, 1);

    esp_timer_create_args_t timer_args = {
        .callback = blink_timer_cb,
        .name = "btn_led_blink",
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_blink_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create blink timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Button LED initialized on GPIO %d (active-LOW)", PIN_LED_BUTTON);
    return ESP_OK;
}

void button_led_set(button_led_mode_t mode)
{
    /* Stop any active blink */
    if (s_blink_timer != NULL) {
        esp_timer_stop(s_blink_timer);  /* Ignore error if not running */
    }

    switch (mode) {
        case BUTTON_LED_OFF:
            gpio_set_level(PIN_LED_BUTTON, 1);  /* OFF = HIGH for active-LOW */
            break;

        case BUTTON_LED_SOLID:
            gpio_set_level(PIN_LED_BUTTON, 0);  /* ON = LOW for active-LOW */
            break;

        case BUTTON_LED_BLINK:
            s_led_state = true;
            gpio_set_level(PIN_LED_BUTTON, 0);  /* Start ON (LOW for active-LOW) */
            /* 250ms interval = 2Hz full cycle (250ms on + 250ms off) */
            esp_timer_start_periodic(s_blink_timer, 250 * 1000);  /* microseconds */
            break;
    }
}

#endif /* BUILD_TARGET_REMOTE */
