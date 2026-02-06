#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "esp_err.h"

/* API - to be implemented */
esp_err_t battery_monitor_init(void);
float battery_get_voltage(void);
uint8_t battery_get_percent(void);
void battery_monitor_task(void *pvParameters);

#endif /* BATTERY_MONITOR_H */
