#ifndef RGB_LED_H
#define RGB_LED_H

#include "esp_err.h"
#include <stdint.h>

/* LED patterns */
typedef enum {
    LED_PATTERN_OFF,
    LED_PATTERN_SOLID,
    LED_PATTERN_BREATHING,
    LED_PATTERN_BLINK_2HZ,
    LED_PATTERN_PULSE_HALF_HZ,
    LED_PATTERN_RAPID_BLINK_5HZ
} led_pattern_t;

/* API - to be implemented */
esp_err_t rgb_led_init(void);
esp_err_t rgb_led_set(uint8_t r, uint8_t g, uint8_t b, led_pattern_t pattern);
void rgb_led_task(void *pvParameters);

#endif /* RGB_LED_H */
