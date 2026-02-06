#ifndef SHARED_QUEUES_H
#define SHARED_QUEUES_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"

/*******************************************************************************
 * Shared FreeRTOS Queues
 *
 * All inter-task communication queues used across the system.
 ******************************************************************************/

/* Queue depths */
#define ESPNOW_RX_QUEUE_DEPTH      16
#define ESPNOW_TX_QUEUE_DEPTH      16
#define ADC_SAMPLE_QUEUE_DEPTH     32
#define LOG_QUEUE_DEPTH            16
#define INPUT_EVENT_QUEUE_DEPTH    8
#define DISPLAY_CMD_QUEUE_DEPTH    8
#define STATE_EVENT_QUEUE_DEPTH    8
#define PING_RESPONSE_QUEUE_DEPTH  2

/* Queue handles - declared extern, defined in shared_queues.c */
extern QueueHandle_t espnow_rx_queue;
extern QueueHandle_t espnow_tx_queue;
extern QueueHandle_t input_event_queue;
extern QueueHandle_t display_cmd_queue;
extern QueueHandle_t state_event_queue;
extern QueueHandle_t ping_response_queue;

#ifdef BUILD_TARGET_BASE
extern QueueHandle_t adc_sample_queue;
extern QueueHandle_t log_queue;
#endif

/*******************************************************************************
 * Shared Semaphores / Mutexes
 ******************************************************************************/
extern SemaphoreHandle_t spi_mutex;
extern SemaphoreHandle_t i2c_mutex;

#ifdef BUILD_TARGET_BASE
extern SemaphoreHandle_t sd_mutex;
#endif

/*******************************************************************************
 * API
 ******************************************************************************/

/**
 * @brief Initialize all shared queues and semaphores.
 *        Must be called before any tasks are created.
 * @return ESP_OK on success, ESP_FAIL if any allocation fails.
 */
esp_err_t shared_queues_init(void);

#endif /* SHARED_QUEUES_H */
