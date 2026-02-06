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

/* API - to be implemented */
esp_err_t adc_as1256_init(void);
esp_err_t adc_as1256_read_channel(uint8_t channel, int32_t *raw_value);
void adc_sampling_task(void *pvParameters);

#endif /* ADC_AS1256_H */
