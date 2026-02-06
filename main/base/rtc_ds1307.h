#ifndef RTC_DS1307_H
#define RTC_DS1307_H

#include "esp_err.h"
#include <time.h>

/* API - to be implemented */
esp_err_t rtc_ds1307_init(void);
esp_err_t rtc_ds1307_get_time(struct tm *time_info);
esp_err_t rtc_ds1307_set_time(const struct tm *time_info);
esp_err_t rtc_ds1307_format_timestamp(char *buf, size_t buf_len);

#endif /* RTC_DS1307_H */
