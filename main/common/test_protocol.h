#ifndef TEST_PROTOCOL_H
#define TEST_PROTOCOL_H

#include "esp_err.h"
#include <stdbool.h>

/*******************************************************************************
 * Serial Test Protocol
 *
 * Exposes a UART-based command interface for automated on-device testing.
 * Commands are sent as newline-terminated strings over the USB serial console.
 * Responses are JSON-formatted for easy parsing by the test runner.
 *
 * Protocol:
 *   Request:  "TEST <command> [args]\n"
 *   Response: "{\"status\":\"ok\"|\"error\",\"command\":\"<cmd>\",\"data\":{...}}\n"
 *
 * Commands:
 *   TEST PING              - Check device is responsive
 *   TEST INFO              - Get device info (target, version, uptime)
 *   TEST STATE             - Get current state machine state (BASE only)
 *   TEST GPIO READ <pin>   - Read a GPIO pin level
 *   TEST GPIO WRITE <pin> <level> - Write a GPIO pin level
 *   TEST QUEUE STATUS      - Get queue fill levels
 *   TEST HEAP              - Get heap memory stats
 *   TEST TASKS             - List running FreeRTOS tasks
 *   TEST ESPNOW STATUS     - Get ESP-NOW link status
 *   TEST RESET             - Software reset the device
 ******************************************************************************/

/**
 * @brief Initialize the serial test protocol handler.
 *        Installs a UART RX handler that listens for "TEST " prefixed commands.
 * @return ESP_OK on success
 */
esp_err_t test_protocol_init(void);

/**
 * @brief Test protocol processing task.
 *        Reads commands from UART and dispatches responses.
 * @param pvParameters Unused
 */
void test_protocol_task(void *pvParameters);

/**
 * @brief Check if the test protocol is enabled.
 * @return true if test protocol task is running
 */
bool test_protocol_is_active(void);

#endif /* TEST_PROTOCOL_H */
