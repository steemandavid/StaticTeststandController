/*******************************************************************************
 * Common - Shared FreeRTOS Queues and Semaphores
 *
 * Centralizes all inter-task communication primitives.
 * Must be initialized before any FreeRTOS tasks are created.
 ******************************************************************************/

#include "config.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "esp_log.h"

static const char *TAG = "shared_queues";

/* ESP-NOW communication queues (both units) */
QueueHandle_t espnow_rx_queue   = NULL;
QueueHandle_t espnow_tx_queue   = NULL;

/* Input/display/state queues (both units) */
QueueHandle_t input_event_queue = NULL;
QueueHandle_t display_cmd_queue = NULL;
QueueHandle_t state_event_queue = NULL;
QueueHandle_t ping_response_queue = NULL;

#ifdef BUILD_TARGET_BASE
/* BASE-only queues */
QueueHandle_t adc_sample_queue  = NULL;
QueueHandle_t log_queue         = NULL;
#endif

/* Shared mutexes */
SemaphoreHandle_t spi_mutex = NULL;
SemaphoreHandle_t i2c_mutex = NULL;

#ifdef BUILD_TARGET_BASE
SemaphoreHandle_t sd_mutex  = NULL;
#endif

esp_err_t shared_queues_init(void)
{
    ESP_LOGI(TAG, "Initializing shared queues and semaphores");

    /* ESP-NOW queues - carry espnow_packet_t */
    espnow_rx_queue = xQueueCreate(ESPNOW_RX_QUEUE_DEPTH, sizeof(espnow_packet_t));
    if (espnow_rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create espnow_rx_queue");
        return ESP_FAIL;
    }

    espnow_tx_queue = xQueueCreate(ESPNOW_TX_QUEUE_DEPTH, sizeof(espnow_packet_t));
    if (espnow_tx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create espnow_tx_queue");
        return ESP_FAIL;
    }

    /* Input event queue - carries uint8_t input_event_t values */
    input_event_queue = xQueueCreate(INPUT_EVENT_QUEUE_DEPTH, sizeof(uint8_t));
    if (input_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create input_event_queue");
        return ESP_FAIL;
    }

    /* Display command queue - carries espnow_packet_t for display updates */
    display_cmd_queue = xQueueCreate(DISPLAY_CMD_QUEUE_DEPTH, sizeof(espnow_packet_t));
    if (display_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create display_cmd_queue");
        return ESP_FAIL;
    }

    /* State event queue - carries uint8_t state transition triggers */
    state_event_queue = xQueueCreate(STATE_EVENT_QUEUE_DEPTH, sizeof(uint8_t));
    if (state_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create state_event_queue");
        return ESP_FAIL;
    }

    /* Ping response queue - carries uint8_t responses for link monitoring */
    ping_response_queue = xQueueCreate(PING_RESPONSE_QUEUE_DEPTH, sizeof(uint8_t));
    if (ping_response_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create ping_response_queue");
        return ESP_FAIL;
    }

#ifdef BUILD_TARGET_BASE
    /* ADC sample queue - carries raw ADC sample buffers */
    adc_sample_queue = xQueueCreate(ADC_SAMPLE_QUEUE_DEPTH, sizeof(int32_t) * ADC_MAX_CHANNELS);
    if (adc_sample_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create adc_sample_queue");
        return ESP_FAIL;
    }

    /* Log queue - carries log message strings */
    log_queue = xQueueCreate(LOG_QUEUE_DEPTH, sizeof(char) * 128);
    if (log_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create log_queue");
        return ESP_FAIL;
    }
#endif

    /* SPI bus mutex */
    spi_mutex = xSemaphoreCreateMutex();
    if (spi_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create spi_mutex");
        return ESP_FAIL;
    }

    /* I2C bus mutex */
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create i2c_mutex");
        return ESP_FAIL;
    }

#ifdef BUILD_TARGET_BASE
    /* SD card mutex */
    sd_mutex = xSemaphoreCreateMutex();
    if (sd_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sd_mutex");
        return ESP_FAIL;
    }
#endif

    ESP_LOGI(TAG, "All queues and semaphores initialized successfully");
    return ESP_OK;
}
