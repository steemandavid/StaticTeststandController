#ifndef SAFETY_H
#define SAFETY_H

#include "esp_err.h"

void safety_enter_safe_state(void);
void safety_watchdog_feed(void);
void safety_watchdog_task(void *pvParameters);

#endif /* SAFETY_H */
