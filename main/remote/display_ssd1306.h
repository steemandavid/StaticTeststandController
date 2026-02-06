#ifndef DISPLAY_SSD1306_H
#define DISPLAY_SSD1306_H

#include "esp_err.h"
#include <stdint.h>

/* Display parameters */
typedef struct {
    char base_state[10];
    uint16_t tx_rx_fails;
    char log_lines[5][22];
} display_params_t;

/* API - to be implemented */
esp_err_t display_init(void);
esp_err_t display_clear(void);
esp_err_t display_update(const display_params_t *params);
esp_err_t display_add_log_line(const char *line);
esp_err_t display_show_sensor_value(const char *label, float value);
void display_update_task(void *pvParameters);

#endif /* DISPLAY_SSD1306_H */
