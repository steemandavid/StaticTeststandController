/*******************************************************************************
 * BASE Unit - ADS1256 24-bit ADC SPI Driver
 *
 * Texas Instruments ADS1256 24-bit Delta-Sigma ADC
 * Provides high-speed data acquisition at up to 1000 Hz.
 * See FSD Section 2.1 for pin assignments and channel map.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_BASE

#include "adc_as1256.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

/* Tag for logging */
static const char *TAG = "adc_ads1256";

/* ADS1256 Register Definitions (Texas Instruments ADS1256) */
#define ADS_REG_STATUS    0x00
#define ADS_REG_MUX       0x01
#define ADS_REG_ADCON     0x02
#define ADS_REG_DRATE     0x03
#define ADS_REG_IO        0x04
#define ADS_REG_OFC0      0x05
#define ADS_REG_OFC1      0x06
#define ADS_REG_OFC2      0x07
#define ADS_REG_FSC0      0x08
#define ADS_REG_FSC1      0x09
#define ADS_REG_FSC2      0x0A

/* ADS1256 Command Definitions */
#define ADS_CMD_WAKEUP    0x00
#define ADS_CMD_RDATA     0x01  /* Read data */
#define ADS_CMD_RDATAC    0x03  /* Read data continuous */
#define ADS_CMD_SDATAC    0x0F  /* Stop read data continuous */
#define ADS_CMD_RREG      0x10  /* Read register */
#define ADS_CMD_WREG      0x50  /* Write register */
#define ADS_CMD_SELFCAL   0xF0  /* Self offset calibration */
#define ADS_CMD_SELFGCAL  0xF1  /* Self gain calibration */
#define ADS_CMD_SYSOCAL   0xF3  /* System offset calibration */
#define ADS_CMD_SYNC      0xFC  /* Synchronize */
#define ADS_CMD_STANDBY   0xFD  /* Standby */
#define ADS_CMD_RESET     0xFE  /* Reset */

/* MUX channel settings (AINP and AINN) */
/* Differential mode: use AIN0-AIN1, AIN2-AIN3, etc. */
/* Single-ended mode: connect negative to AGND (0x08) */
#define ADS_MUX_AIN0      0x00
#define ADS_MUX_AIN1      0x01
#define ADS_MUX_AIN2      0x02
#define ADS_MUX_AIN3      0x03
#define ADS_MUX_AIN4      0x04
#define ADS_MUX_AIN5      0x05
#define ADS_MUX_AIN6      0x06
#define ADS_MUX_AIN7      0x07
#define ADS_MUX_AINCOM    0x08  /* Common (AGND) */

/* Data rate settings (DRATE register) */
#define ADS_DRATE_30000   0xF0  /* 30,000 SPS */
#define ADS_DRATE_15000   0xE0  /* 15,000 SPS */
#define ADS_DRATE_7500    0xD0  /* 7,500 SPS */
#define ADS_DRATE_3750    0xC0  /* 3,750 SPS */
#define ADS_DRATE_2000    0xB0  /* 2,000 SPS */
#define ADS_DRATE_1000    0xA0  /* 1,000 SPS */
#define ADS_DRATE_500     0x90  /* 500 SPS */
#define ADS_DRATE_100     0x50  /* 100 SPS */
#define ADS_DRATE_60      0x40  /* 60 SPS */
#define ADS_DRATE_50      0x30  /* 50 SPS */
#define ADS_DRATE_20      0x20  /* 20 SPS */
#define ADS_DRATE_10      0x10  /* 10 SPS */
#define ADS_DRATE_5       0x00  /* 5 SPS */

/* Default sample rate (reduced from 1000 Hz for stability) */
#define ADS_DRATE_DEFAULT ADS_DRATE_100

/* ADCON register bits */
#define ADS_ADCON_CLKOUT  0x00  /* Clock out enabled */
#define ADS_ADCON_CD_OFF  0x02  /* Sensor detect off */
#define ADS_ADCON_CS_OFF  0x04  /* Current source off */

/* SPI Configuration */
#define ADS_SPI_HOST      SPI2_HOST
#define ADS_SPI_FREQ      1000000  /* 1 MHz (max for ADS1256) */

/* Module state */
static spi_device_handle_t spi_handle = NULL;
static SemaphoreHandle_t drdy_sem = NULL;
static volatile bool adc_initialized = false;

/* Default calibration values (to be loaded from settings) */
static float cal_loadcell = 1.0f;
static float cal_pressure = 1.0f;
static float cal_igniter = 1.0f;
static float cal_breakwire[4] = {1.0f, 1.0f, 1.0f, 1.0f};

/* Channel assignments (from settings) */
static uint8_t port_loadcell = 0;
static uint8_t port_pressure = 1;
static uint8_t port_igniter = 2;
static uint8_t port_breakwire[4] = {3, 4, 5, 6};

/* Forward declarations */
static void IRAM_ATTR drdy_isr_handler(void *arg);
static esp_err_t ads_write_reg(uint8_t reg, uint8_t value);
static esp_err_t ads_read_reg(uint8_t reg, uint8_t *value);
static esp_err_t ads_wait_for_drdy(TickType_t timeout);
static int32_t ads_read_data(void);

/*******************************************************************************
 * DRDY Interrupt Handler
 ******************************************************************************/
static void IRAM_ATTR drdy_isr_handler(void *arg)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (drdy_sem != NULL) {
        xSemaphoreGiveFromISR(drdy_sem, &higher_priority_task_woken);
    }
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/*******************************************************************************
 * Low-Level SPI Operations
 ******************************************************************************/

/* Write to a register */
static esp_err_t ads_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx_data[2];

    tx_data[0] = ADS_CMD_WREG | (reg & 0x0F);  /* Write command */
    tx_data[1] = value;                         /* Data to write */

    spi_transaction_t trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,                            /* 2 bytes */
        .tx_data[0] = tx_data[0],
        .tx_data[1] = tx_data[1],
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write reg 0x%02X: %s", reg, esp_err_to_name(ret));
    }
    return ret;
}

/* Read from a register */
static esp_err_t ads_read_reg(uint8_t reg, uint8_t *value)
{
    uint8_t tx_data[2];
    uint8_t rx_data[2];

    tx_data[0] = ADS_CMD_RREG | (reg & 0x0F);  /* Read command */
    tx_data[1] = 0x00;                          /* Dummy byte */

    spi_transaction_t trans = {
        .flags = 0,
        .length = 16,                            /* 2 bytes */
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read reg 0x%02X: %s", reg, esp_err_to_name(ret));
        return ret;
    }

    *value = rx_data[1];  /* Second byte is the register value */
    return ESP_OK;
}

/* Wait for DRDY to go low */
static esp_err_t ads_wait_for_drdy(TickType_t timeout)
{
    if (drdy_sem != NULL) {
        if (xSemaphoreTake(drdy_sem, timeout) == pdTRUE) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "DRDY timeout");
        return ESP_ERR_TIMEOUT;
    }

    /* Fallback: poll the GPIO pin */
    TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(PIN_ADS_DRDY) == 1) {
        if ((xTaskGetTickCount() - start) >= timeout) {
            ESP_LOGE(TAG, "DRDY poll timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

/* Read 24-bit ADC data */
static int32_t ads_read_data(void)
{
    uint8_t tx_data[3] = {0, 0, 0};
    uint8_t rx_data[3];

    spi_transaction_t trans = {
        .flags = 0,
        .length = 24,                            /* 3 bytes */
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t ret = spi_device_transmit(spi_handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC data: %s", esp_err_to_name(ret));
        return 0;
    }

    /* Convert 3 bytes to signed 32-bit integer */
    int32_t value = ((int32_t)rx_data[0] << 16) |
                    ((int32_t)rx_data[1] << 8) |
                    (int32_t)rx_data[2];

    /* Sign extend from 24-bit to 32-bit */
    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    return value;
}

/*******************************************************************************
 * Public API Implementation
 ******************************************************************************/

esp_err_t adc_as1256_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing ADS1256 ADC...");

    /* Create DRDY semaphore */
    drdy_sem = xSemaphoreCreateBinary();
    if (drdy_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create DRDY semaphore");
        return ESP_ERR_NO_MEM;
    }

    /* Configure DRDY GPIO as input with interrupt */
    gpio_config_t drdy_conf = {
        .pin_bit_mask = (1ULL << PIN_ADS_DRDY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&drdy_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure DRDY GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Install ISR service */
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Add DRDY interrupt handler */
    ret = gpio_isr_handler_add(PIN_ADS_DRDY, drdy_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add DRDY ISR: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure SPI bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_ADS_MOSI,
        .miso_io_num = PIN_ADS_MISO,
        .sclk_io_num = PIN_ADS_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };

    ret = spi_bus_initialize(ADS_SPI_HOST, &buscfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure SPI device */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = ADS_SPI_FREQ,
        .mode = 1,                    /* SPI Mode 1 for ADS1256 */
        .spics_io_num = PIN_ADS_CS,
        .queue_size = 4,
        .flags = SPI_DEVICE_NO_DUMMY,
    };

    ret = spi_bus_add_device(ADS_SPI_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Reset the ADC via RST pin */
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << PIN_ADS_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&rst_conf);

    /* Pulse reset low */
    gpio_set_level(PIN_ADS_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_ADS_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Send RESET command via SPI */
    uint8_t rst_cmd = ADS_CMD_RESET;
    spi_transaction_t rst_trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data[0] = rst_cmd,
    };
    spi_device_transmit(spi_handle, &rst_trans);
    vTaskDelay(pdMS_TO_TICKS(50));  /* Wait for reset to complete */

    /* Configure ADC registers */
    ret = ads_write_reg(ADS_REG_STATUS, 0x04);  /* Enable buffer */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set STATUS register");
        return ret;
    }

    ret = ads_write_reg(ADS_REG_ADCON, ADS_ADCON_CD_OFF | ADS_ADCON_CS_OFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ADCON register");
        return ret;
    }

    ret = ads_write_reg(ADS_REG_DRATE, ADS_DRATE_DEFAULT);  /* 100 SPS default */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set DRATE register");
        return ret;
    }

    /* Send SYNC command to synchronize */
    uint8_t sync_cmd = ADS_CMD_SYNC;
    spi_transaction_t sync_trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data[0] = sync_cmd,
    };
    spi_device_transmit(spi_handle, &sync_trans);

    vTaskDelay(pdMS_TO_TICKS(10));

    /* Wait for DRDY to indicate ready */
    ret = ads_wait_for_drdy(pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADS1256 not responding after init");
        return ret;
    }

    /* Perform self calibration */
    uint8_t cal_cmd = ADS_CMD_SELFCAL;
    spi_transaction_t cal_trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data[0] = cal_cmd,
    };
    spi_device_transmit(spi_handle, &cal_trans);

    /* Wait for calibration to complete (DRDY goes high then low) */
    vTaskDelay(pdMS_TO_TICKS(500));
    ret = ads_wait_for_drdy(pdMS_TO_TICKS(500));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration timeout (may still be OK)");
    }

    adc_initialized = true;
    ESP_LOGI(TAG, "ADS1256 ADC initialized successfully");
    return ESP_OK;
}

esp_err_t adc_as1256_read_channel(uint8_t channel, int32_t *raw_value)
{
    if (!adc_initialized) {
        ESP_LOGE(TAG, "ADC not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (channel >= 8) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (raw_value == NULL) {
        ESP_LOGE(TAG, "NULL value pointer");
        return ESP_ERR_INVALID_ARG;
    }

    /* Configure MUX for single-ended reading from specified channel */
    uint8_t mux_value = (ADS_MUX_AINCOM << 4) | (channel & 0x07);
    esp_err_t ret = ads_write_reg(ADS_REG_MUX, mux_value);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Send SYNC command to trigger conversion */
    uint8_t sync_cmd = ADS_CMD_SYNC;
    spi_transaction_t sync_trans = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data[0] = sync_cmd,
    };
    spi_device_transmit(spi_handle, &sync_trans);

    /* Wait for DRDY */
    ret = ads_wait_for_drdy(pdMS_TO_TICKS(10));
    if (ret != ESP_OK) {
        return ret;
    }

    /* Read data */
    *raw_value = ads_read_data();

    return ESP_OK;
}

void adc_sampling_task(void *pvParameters)
{
    adc_sample_t sample;
    esp_err_t ret;

    ESP_LOGI(TAG, "ADC sampling task started");
    vTaskDelay(pdMS_TO_TICKS(100));

    while (1) {
        if (!adc_initialized) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* Get current timestamp */
        sample.timestamp_us = esp_timer_get_time();

        /* Read all 8 channels */
        for (int i = 0; i < 8; i++) {
            ret = adc_as1256_read_channel(i, &sample.raw_adc[i]);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to read channel %d", i);
                sample.raw_adc[i] = 0;
            }
        }

        /* Convert raw ADC values to engineering units */
        /* Using calibration constants */
        sample.loadcell_kg = (float)sample.raw_adc[port_loadcell] * cal_loadcell;
        sample.pressure_bar = (float)sample.raw_adc[port_pressure] * cal_pressure;
        sample.igniter_v = (float)sample.raw_adc[port_igniter] * cal_igniter;
        for (int i = 0; i < 4; i++) {
            sample.breakwire_v[i] = (float)sample.raw_adc[port_breakwire[i]] * cal_breakwire[i];
        }

        /* TODO: Send sample to logging queue */
        /* For now, just log at low rate for debugging */
        static int counter = 0;
        if (counter++ % 100 == 0) {
            ESP_LOGI(TAG, "Sample: load=%.2fkg, press=%.2fbar, ign=%.2fV",
                     sample.loadcell_kg, sample.pressure_bar, sample.igniter_v);
        }

        /* Delay to maintain target sample rate */
        vTaskDelay(pdMS_TO_TICKS(10));  /* 10ms = 100 Hz */
    }
}

/* Calibration API */
void adc_as1256_set_cal_loadcell(float cal_value)
{
    cal_loadcell = cal_value;
}

void adc_as1256_set_cal_pressure(float cal_value)
{
    cal_pressure = cal_value;
}

void adc_as1256_set_cal_igniter(float cal_value)
{
    cal_igniter = cal_value;
}

void adc_as1256_set_cal_breakwire(uint8_t index, float cal_value)
{
    if (index < 4) {
        cal_breakwire[index] = cal_value;
    }
}

/* Port assignment API */
void adc_as1256_set_port_loadcell(uint8_t port)
{
    if (port < 8) {
        port_loadcell = port;
    }
}

void adc_as1256_set_port_pressure(uint8_t port)
{
    if (port < 8) {
        port_pressure = port;
    }
}

void adc_as1256_set_port_igniter(uint8_t port)
{
    if (port < 8) {
        port_igniter = port;
    }
}

void adc_as1256_set_port_breakwire(uint8_t index, uint8_t port)
{
    if (index < 4 && port < 8) {
        port_breakwire[index] = port;
    }
}

#endif /* BUILD_TARGET_BASE */
