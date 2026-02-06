#ifndef CONFIG_H
#define CONFIG_H

/*******************************************************************************
 * StaticTeststandController - Pin Definitions & Configuration
 *
 * Hardware: ESP32-S3-N16R8 (16MB Flash, 8MB PSRAM)
 * Framework: ESP-IDF v5.0+
 ******************************************************************************/

/* Build target check */
#if !defined(BUILD_TARGET_BASE) && !defined(BUILD_TARGET_REMOTE)
#error "Must define BUILD_TARGET_BASE or BUILD_TARGET_REMOTE"
#endif

/*******************************************************************************
 * MAC Addresses
 ******************************************************************************/
#define BASE_MAC_ADDR   {0x94, 0xa9, 0x90, 0x31, 0x18, 0x38}
#define REMOTE_MAC_ADDR {0x94, 0xa9, 0x90, 0x31, 0x2e, 0x70}

/*******************************************************************************
 * BASE Unit GPIO Assignments
 ******************************************************************************/
#ifdef BUILD_TARGET_BASE

/* AS1256 ADC (SPI) */
#define PIN_ADS_MOSI        35
#define PIN_ADS_SCLK        36
#define PIN_ADS_MISO        37
#define PIN_ADS_RST         38
#define PIN_ADS_CS          39
#define PIN_ADS_DRDY        40

/* SD Card (SPI) */
#define PIN_SD_CS           10
#define PIN_SD_MOSI         11
#define PIN_SD_CLK          12
#define PIN_SD_MISO         13

/* DS1307 RTC (I2C) */
#define PIN_RTC_SDA         8
#define PIN_RTC_SCL         9

/* Igniter */
#define PIN_IGNITION        41
#define PIN_LOW_SIDE_POWER  40

/* Output */
#define PIN_BUZZER          42
#define PIN_RGB_LED         47
#define PIN_LED_BUILTIN     2

#endif /* BUILD_TARGET_BASE */

/*******************************************************************************
 * REMOTE Unit GPIO Assignments
 ******************************************************************************/
#ifdef BUILD_TARGET_REMOTE

/* SSD1306 OLED (I2C) */
#define PIN_I2C_SDA         8
#define PIN_I2C_SCL         9

/* Inputs */
#define PIN_BUTTON          16
#define PIN_LED_BUTTON      17
#define PIN_SWITCH_ARMED    4
#define PIN_SWITCH_SAFE     5

/* Battery */
#define PIN_VOLT_BAT        1

/* Output */
#define PIN_BUZZER          42
#define PIN_RGB_LED         47
#define PIN_LED_BUILTIN     32

#endif /* BUILD_TARGET_REMOTE */

/*******************************************************************************
 * Communication Constants
 ******************************************************************************/
#define ESPNOW_CHANNEL          1
#define MAX_RETRIES             5
#define RETRY_DELAY_MS          100
#define COMMS_WARNING_RETRIES   2
#define PING_INTERVAL_MS        1000
#define RSSI_WARNING_THRESHOLD  -70   /* dBm */
#define RSSI_ERROR_THRESHOLD    -85   /* dBm */
#define RSSI_HISTORY_SIZE       5

/*******************************************************************************
 * Timing Constants
 ******************************************************************************/
#define DEBOUNCE_MS             50
#define LONG_PRESS_MS           2000
#define DOUBLE_PRESS_MS         500
#define WATCHDOG_TIMEOUT_MS     1000

/*******************************************************************************
 * ADC Constants
 ******************************************************************************/
#define ADC_MAX_CHANNELS        8
#define ADC_DEFAULT_SAMPLE_RATE 1000

/*******************************************************************************
 * Display Constants
 ******************************************************************************/
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          64
#define LOG_LINE_MAX_CHARS      21
#define LOG_LINE_COUNT          5

/*******************************************************************************
 * Battery Monitoring (REMOTE)
 ******************************************************************************/
#define BAT_R1                  5600    /* Ohms - top resistor */
#define BAT_R2                  10000   /* Ohms - bottom resistor */
#define BAT_WARNING_PERCENT     30
#define BAT_CRITICAL_PERCENT    10
#define BAT_VOLTAGE_MIN         3.0f    /* 1S LiPo empty */
#define BAT_VOLTAGE_MAX         4.2f    /* 1S LiPo full */

/*******************************************************************************
 * FreeRTOS Task Priorities
 ******************************************************************************/
#define TASK_PRIORITY_WATCHDOG      8
#define TASK_PRIORITY_ADC_SAMPLING  7
#define TASK_PRIORITY_SD_LOGGING    6
#define TASK_PRIORITY_STATE_MACHINE 5
#define TASK_PRIORITY_ESPNOW_RX     5
#define TASK_PRIORITY_ESPNOW_TX     5
#define TASK_PRIORITY_INPUT_HANDLER 5
#define TASK_PRIORITY_CMD_DISPATCH  4
#define TASK_PRIORITY_RGB_LED       4
#define TASK_PRIORITY_BUZZER        3
#define TASK_PRIORITY_PING_MONITOR  3
#define TASK_PRIORITY_DISPLAY       3
#define TASK_PRIORITY_BAT_MONITOR   2

/*******************************************************************************
 * FreeRTOS Task Stack Sizes
 ******************************************************************************/
#define STACK_SIZE_DEFAULT          4096
#define STACK_SIZE_ADC              4096
#define STACK_SIZE_SD_LOGGING       8192
#define STACK_SIZE_DISPLAY          4096
#define STACK_SIZE_BUZZER           3072

/*******************************************************************************
 * Safety Constants
 ******************************************************************************/
#define IGNITER_MAX_CURRENT_A       3.0f
#define IGNITER_SAFETY_MULTIPLIER   2.0f
#define END_BURN_THRESHOLD_PERCENT  5.0f
#define BASELINE_SAMPLE_TIME_S      0.5f

#endif /* CONFIG_H */
