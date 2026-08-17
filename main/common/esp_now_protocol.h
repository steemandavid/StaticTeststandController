#ifndef ESP_NOW_PROTOCOL_H
#define ESP_NOW_PROTOCOL_H

#include <stdint.h>
#include "esp_err.h"

/*******************************************************************************
 * ESP-NOW Packet Structure (25 bytes)
 ******************************************************************************/
typedef struct {
    uint8_t base_state;      /* Current BASE state (0-12) */
    uint8_t command;         /* Command ID */
    int16_t data;            /* Parameter (duration, count, RSSI, etc.) */
    char message[21];        /* Text payload (20 chars + null) */
} __attribute__((packed)) espnow_packet_t;

/*******************************************************************************
 * Command IDs
 ******************************************************************************/

/* Communication (0x00-0x0F) */
#define CMD_PING                0x00
#define CMD_PING_RESPONSE       0x01
#define CMD_COMMS_WARNING       0x02
#define CMD_COMMS_ERROR         0x03
#define CMD_HALT                0x04
#define CMD_IGNITION            0x05  /* Internal: start igniter */
#define CMD_START_RUNNING       0x06  /* Internal: begin test running */
#define CMD_END_TEST            0x07  /* Internal: test complete */
#define CMD_TO_IDLE             0x08  /* Internal: return to idle */

/* Input Commands - REMOTE to BASE (0x10-0x3F) */
#define CMD_SAFE_SHORT_PRESS    0x10
#define CMD_SAFE_LONG_PRESS     0x11
#define CMD_ARM_SHORT_PRESS     0x12
#define CMD_ARM_LONG_PRESS      0x13
#define CMD_SWITCH_TO_ARMED     0x14
#define CMD_SWITCH_TO_SAFE      0x15
#define CMD_BTN_SHORT_PRESS     0x20
#define CMD_BTN_LONG_PRESS      0x21
#define CMD_BTN_DOUBLE_PRESS    0x22
#define CMD_BAT_WARNING         0x30
#define CMD_BAT_CRITICAL        0x31

/* Output Commands - BASE to REMOTE (0x40-0x4F) */
#define CMD_LED_SAFE            0x40
#define CMD_LED_ARMED           0x41
#define CMD_LED_TESTING         0x42
#define CMD_LED_COMPLETE        0x43
#define CMD_LED_ERROR           0x44
#define CMD_LED_SWITCH_ERR      0x45
#define CMD_BUZZER_ON           0x46
#define CMD_BUZZER_OFF          0x47
#define CMD_BUTTON_LED_ON       0x48
#define CMD_BUTTON_LED_OFF      0x49

/* Display Commands - BASE to REMOTE (0x50-0x5F) */
#define CMD_DISPLAY_CLEAR       0x50
#define CMD_DISPLAY_LOG_LINE    0x51
#define CMD_DISPLAY_SENSOR      0x52

/*******************************************************************************
 * API - to be implemented
 ******************************************************************************/
esp_err_t espnow_init(void);
esp_err_t espnow_send_critical(const espnow_packet_t *packet);
esp_err_t espnow_send_normal(const espnow_packet_t *packet);
int8_t espnow_get_last_rssi(void);
void espnow_rx_task(void *pvParameters);
void espnow_tx_task(void *pvParameters);

#endif /* ESP_NOW_PROTOCOL_H */
