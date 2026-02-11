#ifndef ADC_AS1256_H
#define ADC_AS1256_H

#include <stdint.h>
#include "esp_err.h"

/* ADC sample structure */
typedef struct {
    uint64_t timestamp_us;
    int32_t raw_adc[8];
    float loadcell_kg;
    float pressure_bar;
    float igniter_v;
    float breakwire_v[4];
} adc_sample_t;

/* API - Core functions */
esp_err_t adc_as1256_init(void);
esp_err_t adc_as1256_read_channel(uint8_t channel, int32_t *raw_value);
void adc_sampling_task(void *pvParameters);

/* API - Calibration configuration */
void adc_as1256_set_cal_loadcell(float cal_value);
void adc_as1256_set_cal_pressure(float cal_value);
void adc_as1256_set_cal_igniter(float cal_value);
void adc_as1256_set_cal_breakwire(uint8_t index, float cal_value);

/* API - Port assignment configuration */
void adc_as1256_set_port_loadcell(uint8_t port);
void adc_as1256_set_port_pressure(uint8_t port);
void adc_as1256_set_port_igniter(uint8_t port);
void adc_as1256_set_port_breakwire(uint8_t index, uint8_t port);

#endif /* ADC_AS1256_H */
