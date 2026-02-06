/*******************************************************************************
 * Common - Ping/RSSI Monitor
 *
 * Periodically sends CMD_PING and tracks responses to monitor link quality.
 * Posts CMD_COMMS_WARNING or CMD_COMMS_ERROR to state_event_queue when
 * signal degrades or link is lost.
 ******************************************************************************/

#include "config.h"
#include "ping_monitor.h"
#include "esp_now_protocol.h"
#include "shared_queues.h"
#include "safety.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ping_mon";

#define MISS_THRESHOLD 3

static int8_t s_rssi_history[RSSI_HISTORY_SIZE];
static int s_rssi_index = 0;
static int s_rssi_count = 0;
static int s_consecutive_misses = 0;
static bool s_connected = false;

esp_err_t ping_monitor_init(void)
{
    memset(s_rssi_history, 0, sizeof(s_rssi_history));
    s_rssi_index = 0;
    s_rssi_count = 0;
    s_consecutive_misses = 0;
    s_connected = false;

    ESP_LOGI(TAG, "Ping monitor initialized");
    return ESP_OK;
}

int8_t ping_monitor_get_avg_rssi(void)
{
    if (s_rssi_count == 0) {
        return 0;
    }

    int count = (s_rssi_count < RSSI_HISTORY_SIZE) ? s_rssi_count : RSSI_HISTORY_SIZE;
    int32_t sum = 0;
    for (int i = 0; i < count; i++) {
        sum += s_rssi_history[i];
    }
    return (int8_t)(sum / count);
}

bool ping_monitor_is_connected(void)
{
    return s_connected;
}

void ping_monitor_task(void *pvParameters)
{
    uint8_t response;

    ESP_LOGI(TAG, "Ping monitor task started");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(PING_INTERVAL_MS));

#ifdef BUILD_TARGET_REMOTE
        /* Feed watchdog on REMOTE (BASE has state_machine_task doing this) */
        safety_watchdog_feed();
#endif

        /* Send ping */
        espnow_packet_t ping_pkt = {0};
        ping_pkt.command = CMD_PING;
        xQueueSend(espnow_tx_queue, &ping_pkt, pdMS_TO_TICKS(50));

        /* Wait for response with timeout */
        if (xQueueReceive(ping_response_queue, &response,
                          pdMS_TO_TICKS(PING_INTERVAL_MS / 2)) == pdTRUE) {
            /* Got response - record RSSI */
            s_consecutive_misses = 0;
            s_connected = true;

            int8_t rssi = espnow_get_last_rssi();
            s_rssi_history[s_rssi_index] = rssi;
            s_rssi_index = (s_rssi_index + 1) % RSSI_HISTORY_SIZE;
            if (s_rssi_count < RSSI_HISTORY_SIZE) {
                s_rssi_count++;
            }

            int8_t avg_rssi = ping_monitor_get_avg_rssi();
            ESP_LOGD(TAG, "Ping OK, RSSI=%d avg=%d", rssi, avg_rssi);

            /* Check RSSI thresholds */
            if (avg_rssi < RSSI_ERROR_THRESHOLD) {
                ESP_LOGW(TAG, "RSSI below error threshold: %d < %d",
                         avg_rssi, RSSI_ERROR_THRESHOLD);
                uint8_t cmd = CMD_COMMS_ERROR;
                xQueueSend(state_event_queue, &cmd, 0);
            } else if (avg_rssi < RSSI_WARNING_THRESHOLD) {
                ESP_LOGW(TAG, "RSSI below warning threshold: %d < %d",
                         avg_rssi, RSSI_WARNING_THRESHOLD);
                uint8_t cmd = CMD_COMMS_WARNING;
                xQueueSend(state_event_queue, &cmd, 0);
            }
        } else {
            /* No response */
            s_consecutive_misses++;
            ESP_LOGW(TAG, "Ping timeout (%d/%d)", s_consecutive_misses, MISS_THRESHOLD);

            if (s_consecutive_misses >= MISS_THRESHOLD && s_connected) {
                s_connected = false;
                ESP_LOGE(TAG, "Link lost - %d consecutive misses", s_consecutive_misses);
                uint8_t cmd = CMD_COMMS_ERROR;
                xQueueSend(state_event_queue, &cmd, 0);
            }
        }
    }
}
