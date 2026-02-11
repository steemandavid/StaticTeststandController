/*******************************************************************************
 * BASE Unit - Settings File Parser
 *
 * Parses settings.txt from SD card root.
 * Format: KEY VALUE # optional comment
 * See FSD Section 4.2 for complete settings list.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_BASE

#include "settings.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Tag for logging */
static const char *TAG = "settings";

/* Settings file path */
#define SETTINGS_PATH "/sdcard/settings.txt"

/* Maximum line length in settings file */
#define MAX_LINE_LEN 256

/* Module state */
static settings_t current_settings = {0};
static bool settings_loaded = false;

/*******************************************************************************
 * Internal Helper Functions
 ******************************************************************************/

/* Trim leading whitespace */
static char *trim_left(char *str)
{
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
        str++;
    }
    return str;
}

/* Trim trailing whitespace */
static char *trim_right(char *str)
{
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        end--;
    }
    *(end + 1) = '\0';
    return str;
}

/* Find comment start (# or //) and truncate */
static void strip_comment(char *str)
{
    char *comment = strchr(str, '#');
    if (comment != NULL) {
        *comment = '\0';
        return;
    }

    /* Check for // style comment */
    comment = strstr(str, "//");
    if (comment != NULL) {
        *comment = '\0';
    }
}

/* Parse a key-value pair line */
static esp_err_t parse_setting_line(const char *line, settings_t *settings)
{
    char line_copy[MAX_LINE_LEN];
    char *key, *value, *saveptr;

    strncpy(line_copy, line, MAX_LINE_LEN - 1);
    line_copy[MAX_LINE_LEN - 1] = '\0';

    /* Strip comments */
    strip_comment(line_copy);

    /* Trim whitespace */
    char *trimmed = trim_left(trim_right(line_copy));

    /* Skip empty lines */
    if (strlen(trimmed) == 0) {
        return ESP_OK;
    }

    /* Split on whitespace */
    key = strtok_r(trimmed, " \t", &saveptr);
    if (key == NULL) {
        return ESP_OK;  /* Empty line */
    }

    value = strtok_r(NULL, " \t", &saveptr);
    if (value == NULL) {
        ESP_LOGW(TAG, "Missing value for key: %s", key);
        return ESP_ERR_INVALID_ARG;
    }

    /* Parse key-value pairs */
    if (strcmp(key, "IGNITER_ON_TIME") == 0) {
        settings->igniter_on_time = atof(value);
    } else if (strcmp(key, "ADC_PORT_LOADCELL") == 0) {
        settings->adc_port_loadcell = (uint8_t)atoi(value);
    } else if (strcmp(key, "ADC_PORT_PRESSURE") == 0) {
        settings->adc_port_pressure = (uint8_t)atoi(value);
    } else if (strcmp(key, "ADC_PORT_IGNITER_SENSE") == 0) {
        settings->adc_port_igniter_sense = (uint8_t)atoi(value);
    } else if (strcmp(key, "ADC_PORT_BREAKWIRE") == 0) {
        /* Parse all 4 breakwire ports: ADC_PORT_BREAKWIRE 0 1 2 3 */
        for (int i = 0; i < 4; i++) {
            char *port_str = strtok_r(NULL, " \t", &saveptr);
            if (port_str != NULL) {
                settings->adc_port_breakwire[i] = (uint8_t)atoi(port_str);
            }
        }
    } else if (strcmp(key, "WIFI_SSID") == 0) {
        strncpy(settings->wifi_ssid, value, sizeof(settings->wifi_ssid) - 1);
    } else if (strcmp(key, "WIFI_PASSWORD") == 0) {
        strncpy(settings->wifi_password, value, sizeof(settings->wifi_password) - 1);
    } else if (strcmp(key, "ADC_SAMPLE_RATE") == 0) {
        settings->adc_sample_rate = (uint16_t)atoi(value);
    } else if (strcmp(key, "ADC_CAL_LOADCELL") == 0) {
        settings->adc_cal_loadcell = atof(value);
    } else if (strcmp(key, "ADC_CAL_PRESSURE") == 0) {
        settings->adc_cal_pressure = atof(value);
    } else if (strcmp(key, "ADC_CAL_IGNITER") == 0) {
        settings->adc_cal_igniter = atof(value);
    } else if (strcmp(key, "ADC_CAL_BREAKWIRE") == 0) {
        /* Parse all 4 breakwire cal values: ADC_CAL_BREAKWIRE 1.0 1.0 1.0 1.0 */
        for (int i = 0; i < 4; i++) {
            char *cal_str = strtok_r(NULL, " \t", &saveptr);
            if (cal_str != NULL) {
                settings->adc_cal_breakwire[i] = atof(cal_str);
            }
        }
    } else if (strcmp(key, "COMMS_WARNING_TIMEOUT") == 0) {
        settings->comms_warning_timeout = (uint8_t)atoi(value);
    } else if (strcmp(key, "COMMS_ERROR_TIMEOUT") == 0) {
        settings->comms_error_timeout = (uint8_t)atoi(value);
    } else if (strcmp(key, "END_TEST_DELAY") == 0) {
        settings->end_test_delay = (uint8_t)atoi(value);
    } else {
        ESP_LOGW(TAG, "Unknown setting key: %s", key);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGD(TAG, "Parsed: %s = %s", key, value);
    return ESP_OK;
}

/* Validate settings values */
static esp_err_t validate_settings(const settings_t *settings)
{
    bool valid = true;

    /* Check igniter on time (0.1 - 10 seconds) */
    if (settings->igniter_on_time < 0.1f || settings->igniter_on_time > 10.0f) {
        ESP_LOGE(TAG, "Invalid IGNITER_ON_TIME: %.2f (range: 0.1 - 10.0)", settings->igniter_on_time);
        valid = false;
    }

    /* Check ADC port assignments (0 - 7) */
    if (settings->adc_port_loadcell >= 8) {
        ESP_LOGE(TAG, "Invalid ADC_PORT_LOADCELL: %d (range: 0 - 7)", settings->adc_port_loadcell);
        valid = false;
    }
    if (settings->adc_port_pressure >= 8) {
        ESP_LOGE(TAG, "Invalid ADC_PORT_PRESSURE: %d (range: 0 - 7)", settings->adc_port_pressure);
        valid = false;
    }
    if (settings->adc_port_igniter_sense >= 8) {
        ESP_LOGE(TAG, "Invalid ADC_PORT_IGNITER_SENSE: %d (range: 0 - 7)", settings->adc_port_igniter_sense);
        valid = false;
    }
    for (int i = 0; i < 4; i++) {
        if (settings->adc_port_breakwire[i] >= 8) {
            ESP_LOGE(TAG, "Invalid ADC_PORT_BREAKWIRE[%d]: %d (range: 0 - 7)",
                     i, settings->adc_port_breakwire[i]);
            valid = false;
        }
    }

    /* Check ADC sample rate (10 - 10000 Hz) */
    if (settings->adc_sample_rate < 10 || settings->adc_sample_rate > 10000) {
        ESP_LOGE(TAG, "Invalid ADC_SAMPLE_RATE: %d (range: 10 - 10000)", settings->adc_sample_rate);
        valid = false;
    }

    /* Check calibration values */
    if (settings->adc_cal_loadcell == 0.0f) {
        ESP_LOGW(TAG, "ADC_CAL_LOADCELL is 0.0 (uncalibrated)");
    }
    if (settings->adc_cal_pressure == 0.0f) {
        ESP_LOGW(TAG, "ADC_CAL_PRESSURE is 0.0 (uncalibrated)");
    }

    /* Check communication timeouts */
    if (settings->comms_warning_timeout == 0 || settings->comms_warning_timeout > 60) {
        ESP_LOGE(TAG, "Invalid COMMS_WARNING_TIMEOUT: %d (range: 1 - 60)", settings->comms_warning_timeout);
        valid = false;
    }
    if (settings->comms_error_timeout < settings->comms_warning_timeout || settings->comms_error_timeout > 120) {
        ESP_LOGE(TAG, "Invalid COMMS_ERROR_TIMEOUT: %d (must be > WARNING and <= 120)", settings->comms_error_timeout);
        valid = false;
    }

    /* Check end test delay (0 - 60 seconds) */
    if (settings->end_test_delay > 60) {
        ESP_LOGE(TAG, "Invalid END_TEST_DELAY: %d (range: 0 - 60)", settings->end_test_delay);
        valid = false;
    }

    return valid ? ESP_OK : ESP_ERR_INVALID_ARG;
}

/*******************************************************************************
 * Public API Implementation
 ******************************************************************************/

esp_err_t settings_load(const char *filepath, settings_t *settings)
{
    FILE *f;
    char line[MAX_LINE_LEN];
    int line_num = 0;
    esp_err_t ret;

    ESP_LOGI(TAG, "Loading settings from: %s", filepath ? filepath : SETTINGS_PATH);

    /* Use default path if not specified */
    const char *path = filepath ? filepath : SETTINGS_PATH;

    /* Open file */
    f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open settings file: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    /* Initialize settings with zeros */
    memset(settings, 0, sizeof(settings_t));

    /* Set default values */
    settings->igniter_on_time = 0.5f;
    settings->adc_port_loadcell = 0;
    settings->adc_port_pressure = 1;
    settings->adc_port_igniter_sense = 2;
    for (int i = 0; i < 4; i++) {
        settings->adc_port_breakwire[i] = 3 + i;
    }
    settings->adc_sample_rate = 1000;
    settings->adc_cal_loadcell = 1.0f;
    settings->adc_cal_pressure = 1.0f;
    settings->adc_cal_igniter = 1.0f;
    for (int i = 0; i < 4; i++) {
        settings->adc_cal_breakwire[i] = 1.0f;
    }
    settings->comms_warning_timeout = 5;
    settings->comms_error_timeout = 10;
    settings->end_test_delay = 5;

    /* Read file line by line */
    while (fgets(line, sizeof(line), f) != NULL) {
        line_num++;
        ret = parse_setting_line(line, settings);
        if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGW(TAG, "Skipping invalid line %d", line_num);
        }
    }

    fclose(f);

    /* Validate settings */
    ret = validate_settings(settings);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Settings validation failed");
        return ret;
    }

    /* Store settings globally */
    memcpy(&current_settings, settings, sizeof(settings_t));
    settings_loaded = true;

    ESP_LOGI(TAG, "Settings loaded successfully (line %d)", line_num);
    return ESP_OK;
}

const settings_t *settings_get(void)
{
    if (!settings_loaded) {
        ESP_LOGW(TAG, "Settings not loaded, returning default values");
        return &current_settings;
    }
    return &current_settings;
}

/* Utility function to print current settings */
void settings_print(void)
{
    const settings_t *s = settings_get();

    ESP_LOGI(TAG, "=== Current Settings ===");
    ESP_LOGI(TAG, "Igniter On Time: %.2f s", s->igniter_on_time);
    ESP_LOGI(TAG, "ADC Ports: Loadcell=%d, Pressure=%d, Igniter=%d",
             s->adc_port_loadcell, s->adc_port_pressure, s->adc_port_igniter_sense);
    ESP_LOGI(TAG, "            Breakwires=[%d,%d,%d,%d]",
             s->adc_port_breakwire[0], s->adc_port_breakwire[1],
             s->adc_port_breakwire[2], s->adc_port_breakwire[3]);
    ESP_LOGI(TAG, "ADC Sample Rate: %d Hz", s->adc_sample_rate);
    ESP_LOGI(TAG, "ADC Cal: Loadcell=%.6f, Pressure=%.6f, Igniter=%.6f",
             s->adc_cal_loadcell, s->adc_cal_pressure, s->adc_cal_igniter);
    ESP_LOGI(TAG, "         Breakwires=[%.6f,%.6f,%.6f,%.6f]",
             s->adc_cal_breakwire[0], s->adc_cal_breakwire[1],
             s->adc_cal_breakwire[2], s->adc_cal_breakwire[3]);
    ESP_LOGI(TAG, "Comms Timeouts: Warning=%d s, Error=%d s",
             s->comms_warning_timeout, s->comms_error_timeout);
    ESP_LOGI(TAG, "End Test Delay: %d s", s->end_test_delay);
    ESP_LOGI(TAG, "WiFi SSID: %s", s->wifi_ssid);
}

#endif /* BUILD_TARGET_BASE */
