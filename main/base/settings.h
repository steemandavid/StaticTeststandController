#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include "esp_err.h"

/* Settings structure */
typedef struct {
    float igniter_on_time;
    uint8_t adc_port_loadcell;
    uint8_t adc_port_pressure;
    uint8_t adc_port_igniter_sense;
    uint8_t adc_port_breakwire[4];
    char wifi_ssid[64];
    char wifi_password[64];
    uint16_t adc_sample_rate;
    float adc_cal_loadcell;
    float adc_cal_pressure;
    float adc_cal_igniter;
    float adc_cal_breakwire[4];
    uint8_t comms_warning_timeout;
    uint8_t comms_error_timeout;
    uint8_t end_test_delay;
} settings_t;

/* API */
esp_err_t settings_load(const char *filepath, settings_t *settings);
const settings_t *settings_get(void);
void settings_print(void);

#endif /* SETTINGS_H */
