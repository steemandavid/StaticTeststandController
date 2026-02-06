/*******************************************************************************
 * Common - Safety Module
 *
 * Implements safe state function and watchdog monitoring.
 * See FSD Appendix C for safety requirements.
 ******************************************************************************/

#include "config.h"
#include "safety.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "safety";

static volatile uint32_t s_heartbeat = 0;

void safety_watchdog_feed(void)
{
    s_heartbeat++;
}

void safety_enter_safe_state(void)
{
    ESP_LOGW(TAG, "*** ENTERING SAFE STATE ***");

#ifdef BUILD_TARGET_BASE
    /* Igniter OFF */
    gpio_set_level(PIN_IGNITION, 0);
    /* Low-side power OFF */
    gpio_set_level(PIN_LOW_SIDE_POWER, 0);
    /* Brief buzzer alert */
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_BUZZER, 0);
#endif

#ifdef BUILD_TARGET_REMOTE
    /* Button LED off */
    gpio_set_level(PIN_LED_BUTTON, 0);
    /* Brief buzzer alert */
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_BUZZER, 0);
#endif

    /* Notify the other unit */
    espnow_packet_t pkt = {0};
    pkt.command = CMD_HALT;
    strncpy(pkt.message, "SAFE STATE", sizeof(pkt.message) - 1);
    xQueueSend(espnow_tx_queue, &pkt, pdMS_TO_TICKS(100));

    ESP_LOGW(TAG, "Safe state entered");
}

void safety_watchdog_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Watchdog task started");
    uint32_t last_heartbeat = 0;

    /* Give system time to start up */
    vTaskDelay(pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS * 2));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS / 2));

        uint32_t current = s_heartbeat;
        if (current == last_heartbeat) {
            ESP_LOGE(TAG, "Watchdog timeout - no heartbeat");
            safety_enter_safe_state();
        }
        last_heartbeat = current;
    }
}
