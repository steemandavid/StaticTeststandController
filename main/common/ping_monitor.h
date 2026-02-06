#ifndef PING_MONITOR_H
#define PING_MONITOR_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

esp_err_t ping_monitor_init(void);
void ping_monitor_task(void *pvParameters);
int8_t ping_monitor_get_avg_rssi(void);
bool ping_monitor_is_connected(void);

#endif /* PING_MONITOR_H */
