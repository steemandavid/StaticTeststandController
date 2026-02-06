#ifndef BUZZER_H
#define BUZZER_H

#include "esp_err.h"

typedef enum {
    BUZZER_PATTERN_OFF,
    BUZZER_PATTERN_SHORT_BEEP,   /* 100ms on */
    BUZZER_PATTERN_LONG_BEEP,    /* 500ms on */
    BUZZER_PATTERN_DOUBLE_BEEP,  /* 100ms on, 100ms off, 100ms on */
    BUZZER_PATTERN_ALARM         /* continuous 250ms on/off */
} buzzer_pattern_t;

esp_err_t buzzer_init(void);
void buzzer_play(buzzer_pattern_t pattern);
void buzzer_stop(void);
void buzzer_task(void *pvParameters);

#endif /* BUZZER_H */
