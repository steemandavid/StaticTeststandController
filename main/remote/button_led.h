#ifndef BUTTON_LED_H
#define BUTTON_LED_H

#include "esp_err.h"

typedef enum {
    BUTTON_LED_OFF,
    BUTTON_LED_SOLID,
    BUTTON_LED_BLINK   /* 2Hz blink via esp_timer */
} button_led_mode_t;

esp_err_t button_led_init(void);
void button_led_set(button_led_mode_t mode);

#endif /* BUTTON_LED_H */
