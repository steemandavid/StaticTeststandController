/*******************************************************************************
 * Common - ESP-NOW Communication Protocol
 *
 * Bidirectional communication between BASE and REMOTE units.
 * See FSD Section 6 for protocol specifications.
 ******************************************************************************/

#include "config.h"
#include "esp_now_protocol.h"
#include "shared_queues.h"

#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "espnow";

#ifdef BUILD_TARGET_BASE
static const uint8_t s_peer_mac[6] = REMOTE_MAC_ADDR;
#else
static const uint8_t s_peer_mac[6] = BASE_MAC_ADDR;
#endif

static volatile esp_now_send_status_t s_send_status = ESP_NOW_SEND_FAIL;
static SemaphoreHandle_t s_send_sem = NULL;
static volatile int8_t s_last_rssi = 0;

/*******************************************************************************
 * Callbacks
 ******************************************************************************/

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    s_send_status = status;
    if (s_send_sem != NULL) {
        xSemaphoreGiveFromISR(s_send_sem, NULL);
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                            const uint8_t *data, int len)
{
    if (len != sizeof(espnow_packet_t)) {
        ESP_LOGW(TAG, "Unexpected packet size: %d", len);
        return;
    }

    /* Capture RSSI from receive control info */
    if (recv_info->rx_ctrl != NULL) {
        s_last_rssi = recv_info->rx_ctrl->rssi;
    }

    espnow_packet_t packet;
    memcpy(&packet, data, sizeof(espnow_packet_t));

    BaseType_t higher_prio_woken = pdFALSE;
    if (xQueueSendFromISR(espnow_rx_queue, &packet, &higher_prio_woken) != pdTRUE) {
        ESP_LOGW(TAG, "RX queue full, dropping cmd=0x%02x", packet.command);
    }
    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

/*******************************************************************************
 * Initialization
 ******************************************************************************/

esp_err_t espnow_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP-NOW");

    s_send_sem = xSemaphoreCreateBinary();
    if (s_send_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create send semaphore");
        return ESP_FAIL;
    }

    /* WiFi in STA mode (required for ESP-NOW) */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    /* Add peer */
    esp_now_peer_info_t peer = {
        .channel = ESPNOW_CHANNEL,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, s_peer_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW initialized, peer added");
    return ESP_OK;
}

/*******************************************************************************
 * Send Functions
 ******************************************************************************/

int8_t espnow_get_last_rssi(void)
{
    return s_last_rssi;
}

esp_err_t espnow_send_critical(const espnow_packet_t *packet)
{
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        esp_err_t ret = esp_now_send(s_peer_mac, (const uint8_t *)packet,
                                      sizeof(espnow_packet_t));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_now_send failed: %s (attempt %d/%d)",
                     esp_err_to_name(ret), attempt + 1, MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            continue;
        }

        if (xSemaphoreTake(s_send_sem, pdMS_TO_TICKS(RETRY_DELAY_MS)) == pdTRUE &&
            s_send_status == ESP_NOW_SEND_SUCCESS) {
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Not acknowledged (attempt %d/%d)", attempt + 1, MAX_RETRIES);
        vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    }

    ESP_LOGE(TAG, "Critical send failed after %d retries, cmd=0x%02x",
             MAX_RETRIES, packet->command);
    return ESP_FAIL;
}

esp_err_t espnow_send_normal(const espnow_packet_t *packet)
{
    esp_err_t ret = esp_now_send(s_peer_mac, (const uint8_t *)packet,
                                  sizeof(espnow_packet_t));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Normal send failed: %s, cmd=0x%02x",
                 esp_err_to_name(ret), packet->command);
    }
    return ret;
}

/*******************************************************************************
 * FreeRTOS Tasks
 ******************************************************************************/

void espnow_rx_task(void *pvParameters)
{
    espnow_packet_t packet;
    ESP_LOGI(TAG, "RX task started");

    for (;;) {
        if (xQueueReceive(espnow_rx_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGD(TAG, "RX: cmd=0x%02x state=%d data=%d",
                 packet.command, packet.base_state, packet.data);

        if (packet.command == CMD_HALT) {
            uint8_t cmd = packet.command;
            xQueueSendToFront(state_event_queue, &cmd, 0);
        } else if (packet.command >= CMD_DISPLAY_CLEAR &&
                   packet.command <= CMD_DISPLAY_SENSOR) {
            xQueueSend(display_cmd_queue, &packet, pdMS_TO_TICKS(50));
        } else if (packet.command >= CMD_SAFE_SHORT_PRESS &&
                   packet.command <= CMD_BAT_CRITICAL) {
            uint8_t cmd = packet.command;
            xQueueSend(state_event_queue, &cmd, pdMS_TO_TICKS(50));
        } else if (packet.command >= CMD_LED_SAFE &&
                   packet.command <= CMD_BUTTON_LED_OFF) {
            uint8_t cmd = packet.command;
            xQueueSend(input_event_queue, &cmd, pdMS_TO_TICKS(50));
        } else if (packet.command == CMD_PING) {
            espnow_packet_t response = {0};
            response.command = CMD_PING_RESPONSE;
            xQueueSend(espnow_tx_queue, &response, 0);
        } else if (packet.command == CMD_PING_RESPONSE) {
            uint8_t cmd = packet.command;
            xQueueSend(ping_response_queue, &cmd, 0);
        }
    }
}

void espnow_tx_task(void *pvParameters)
{
    espnow_packet_t packet;
    ESP_LOGI(TAG, "TX task started");

    for (;;) {
        if (xQueueReceive(espnow_tx_queue, &packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGD(TAG, "TX: cmd=0x%02x state=%d",
                 packet.command, packet.base_state);

        if (packet.command == CMD_HALT ||
            packet.command == CMD_SWITCH_TO_ARMED ||
            packet.command == CMD_SWITCH_TO_SAFE) {
            espnow_send_critical(&packet);
        } else {
            espnow_send_normal(&packet);
        }
    }
}
