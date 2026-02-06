#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "esp_err.h"

/* Input event types */
typedef enum {
    INPUT_BUTTON_SHORT_PRESS,
    INPUT_BUTTON_LONG_PRESS,
    INPUT_BUTTON_DOUBLE_PRESS,
    INPUT_SWITCH_TO_ARMED,
    INPUT_SWITCH_TO_SAFE,
    INPUT_SWITCH_ERROR
} input_event_t;

/* API - to be implemented */
esp_err_t input_handler_init(void);
void input_handler_task(void *pvParameters);

#endif /* INPUT_HANDLER_H */
