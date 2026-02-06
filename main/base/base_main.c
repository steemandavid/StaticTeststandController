/*******************************************************************************
 * BASE Unit - Main Entry Point
 *
 * Initializes all BASE subsystems and starts FreeRTOS tasks.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_BASE

#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "state_machine.h"
#include "rgb_led.h"
#include "buzzer.h"
#include "ping_monitor.h"
#include "safety.h"
#include "test_protocol.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "base_main";

static void init_gpio(void)
{
    /* Igniter output - start OFF */
    gpio_config_t ign_cfg = {
        .pin_bit_mask = (1ULL << PIN_IGNITION) | (1ULL << PIN_LOW_SIDE_POWER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ign_cfg);
    gpio_set_level(PIN_IGNITION, 0);
    gpio_set_level(PIN_LOW_SIDE_POWER, 0);

    /* Buzzer output - active low, start OFF (high) */
    gpio_config_t buz_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&buz_cfg);
    gpio_set_level(PIN_BUZZER, 1);
}

void base_main(void)
{
    ESP_LOGI(TAG, "BASE unit starting");

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

    /* Initialize RGB LED */
    ESP_ERROR_CHECK(rgb_led_init());

    /* Initialize buzzer */
    ESP_ERROR_CHECK(buzzer_init());

    /* Initialize ping monitor */
    ESP_ERROR_CHECK(ping_monitor_init());

    /* Initialize state machine */
    state_machine_init();

    ESP_LOGI(TAG, "Creating FreeRTOS tasks");

    /* Watchdog - highest priority */
    xTaskCreate(safety_watchdog_task, "watchdog",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_WATCHDOG, NULL);

    /* State machine */
    xTaskCreate(state_machine_task, "state_machine",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_STATE_MACHINE, NULL);

    /* ESP-NOW RX/TX */
    xTaskCreate(espnow_rx_task, "espnow_rx",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_ESPNOW_RX, NULL);

    xTaskCreate(espnow_tx_task, "espnow_tx",
                STACK_SIZE_DEFAULT, NULL,
                TASK_PRIORITY_ESPNOW_TX, NULL);

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

    /* Serial test protocol (automated testing interface) */
    ESP_ERROR_CHECK(test_protocol_init());
    xTaskCreate(test_protocol_task, "test_proto",
                STACK_SIZE_DEFAULT, NULL,
                1, NULL);  /* Lowest priority - background service */

    ESP_LOGI(TAG, "BASE unit initialized, tasks running");
}

#endif /* BUILD_TARGET_BASE */
