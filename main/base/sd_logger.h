#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include "esp_err.h"
#include "adc_as1256.h"

/* API - to be implemented */
esp_err_t sd_logger_init(void);
esp_err_t sd_logger_create_test_file(const char *timestamp);
esp_err_t sd_logger_write_sample(const adc_sample_t *sample);
esp_err_t sd_logger_write_summary(float duration, float max_thrust,
                                   float total_impulse, float max_pressure);
esp_err_t sd_logger_close(void);
void sd_logging_task(void *pvParameters);

#endif /* SD_LOGGER_H */
