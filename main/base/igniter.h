#ifndef IGNITER_H
#define IGNITER_H

#include "esp_err.h"
#include <stdbool.h>

/* API - to be implemented */
esp_err_t igniter_init(void);
esp_err_t igniter_fire(float duration_s);
void igniter_cutoff(void);
bool igniter_check_continuity(float *voltage);

#endif /* IGNITER_H */
