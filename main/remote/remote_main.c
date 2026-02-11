/*******************************************************************************
 * REMOTE Unit - Main Entry Point
 *
 * Initializes all REMOTE subsystems and starts FreeRTOS tasks.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_REMOTE

#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "input_handler.h"
#include "display_ssd1306.h"
#include "rgb_led.h"
#include "buzzer.h"
#include "ping_monitor.h"
#include "button_led.h"
#include "cmd_dispatch.h"
#include "safety.h"
#include "test_protocol.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "remote_main";

static void init_gpio(void)
{
    /* Button LED output - active HIGH, start OFF (low) */
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << PIN_LED_BUTTON),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    gpio_set_level(PIN_LED_BUTTON, 0);  /* OFF = low for active-HIGH */

    /* Buzzer output - active LOW, start OFF (HIGH) */
    gpio_config_t buz_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&buz_cfg);
    gpio_set_level(PIN_BUZZER, 1);  /* OFF = HIGH for active-LOW */
}

void remote_main(void)
{
    ESP_LOGI(TAG, "REMOTE unit starting");

    /* Initialize NVS (required for WiFi/ESP-NOW) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize shared queues and semaphores */
    ESP_ERROR_CHECK(shared_queues_init());

    /* Initialize GPIO */
    init_gpio();

    /* Initialize ESP-NOW */
    ESP_ERROR_CHECK(espnow_init());

    /* Initialize input handler */
    ESP_ERROR_CHECK(input_handler_init());

    /* Initialize display */
    ESP_ERROR_CHECK(display_init());

    /* Initialize RGB LED */
    ESP_ERROR_CHECK(rgb_led_init());

    /* Initialize buzzer */
    ESP_ERROR_CHECK(buzzer_init());

    /* Initialize ping monitor */
    ESP_ERROR_CHECK(ping_monitor_init());

    /* Initialize button LED */
    ESP_ERROR_CHECK(button_led_init());

    ESP_LOGI(TAG, "Creating FreeRTOS tasks");

    /* Watchdog - highest priority */
    xTaskCreate(safety_watchdog_task, "watchdog",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_WATCHDOG, NULL);

    /* ESP-NOW RX/TX */
    xTaskCreate(espnow_rx_task, "espnow_rx",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_ESPNOW_RX, NULL);

    xTaskCreate(espnow_tx_task, "espnow_tx",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_ESPNOW_TX, NULL);

    /* Input handler */
    xTaskCreate(input_handler_task, "input",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_INPUT_HANDLER, NULL);

    /* Display update */
    xTaskCreate(display_update_task, "display",
                STACK_SIZE_DISPLAY, NULL,
                TASK_PRIORITY_DISPLAY, NULL);

    /* RGB LED */
    xTaskCreate(rgb_led_task, "rgb_led",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_RGB_LED, NULL);

    /* Buzzer */
    xTaskCreate(buzzer_task, "buzzer",
                STACK_SIZE_BUZZER, NULL,
                TASK_PRIORITY_BUZZER, NULL);

    /* Ping monitor */
    xTaskCreate(ping_monitor_task, "ping_mon",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_PING_MONITOR, NULL);

    /* Command dispatch - routes BASE commands to LED/buzzer/button */
    xTaskCreate(cmd_dispatch_task, "cmd_dispatch",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_CMD_DISPATCH, NULL);

    /* Serial test protocol (automated testing interface) */
    ESP_ERROR_CHECK(test_protocol_init());
    xTaskCreate(test_protocol_task, "test_proto",
                STACK_SIZE_TEST_PROTO, NULL,
                TASK_PRIORITY_TEST_PROTO, NULL);

    /* Startup notification: 3 beeps + LED flash pattern */
    ESP_LOGI(TAG, "Playing startup notification (GPIO mode for quick beeps)");

    /* Use direct GPIO for boot beeps (buzzer task not ready yet) */
    /* Active-LOW buzzer: LOW = ON, HIGH = OFF */
    for (int i = 0; i < 3; i++) {
        gpio_set_level(PIN_BUZZER, 0);  /* ON (active-LOW) */
        vTaskDelay(pdMS_TO_TICKS(150));
        gpio_set_level(PIN_BUZZER, 1);  /* OFF (active-LOW) */
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    rgb_led_set(0, 255, 0, LED_PATTERN_SOLID);  /* Green */
    vTaskDelay(pdMS_TO_TICKS(200));

    rgb_led_set(255, 255, 0, LED_PATTERN_SOLID);  /* Yellow */
    vTaskDelay(pdMS_TO_TICKS(200));

    rgb_led_set(0, 0, 255, LED_PATTERN_SOLID);  /* Blue */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Return to default state color: green breathing = waiting */
    rgb_led_set(0, 255, 0, LED_PATTERN_BREATHING);

    ESP_LOGI(TAG, "REMOTE unit initialized, tasks running");
}

#endif /* BUILD_TARGET_REMOTE */
