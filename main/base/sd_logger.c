/*******************************************************************************
 * BASE Unit - SD Card Logger
 *
 * Handles FAT32 mounting, CSV file creation, and data logging.
 * See FSD Section 4.3 for file format specifications.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_BASE

#include "sd_logger.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

/* Tag for logging */
static const char *TAG = "sd_logger";

/* SD card mount point */
#define MOUNT_POINT "/sdcard"

/* Maximum path length */
#define MAX_PATH_LEN 128

/* Module state */
static bool sd_mounted = false;
static FILE *current_file = NULL;
static char current_filename[MAX_PATH_LEN];
static uint32_t sample_count = 0;
static SemaphoreHandle_t sd_mutex = NULL;
static sdmmc_card_t *card = NULL;

/* SPI Configuration for SD card */
#define SD_SPI_HOST      SPI2_HOST  /* Shared with AS1256 but different CS */
#define SD_SPI_FREQ      16000000   /* 16 MHz for SD card */

/* Forward declarations */
static void create_csv_header(FILE *f);

/*******************************************************************************
 * Public API Implementation
 ******************************************************************************/

esp_err_t sd_logger_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing SD card logger...");

    /* Create mutex for SD card access */
    sd_mutex = xSemaphoreCreateMutex();
    if (sd_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create SD mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Configure SPI bus for SD card (may already be initialized by ADC) */
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092,
    };

    /* Try to initialize SPI bus (may already be initialized) */
    ret = spi_bus_initialize(SD_SPI_HOST, &buscfg, SPI_DMA_DISABLED);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure SD card slot */
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = SD_SPI_HOST;

    /* Configure SPI host for SD card */
    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = SD_SPI_HOST;
    host_config.max_freq_khz = SD_SPI_FREQ / 1000;

    /* Mount FAT filesystem - ESP-IDF v5.5.2 API */
    esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 5,
        .format_if_mount_failed = false,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card_out;
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host_config, &slot_config, &mount_config, &card_out);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        return ret;
    }

    card = card_out;
    sd_mounted = true;

    /* Print SD card information */
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD card mounted successfully at %s", MOUNT_POINT);

    return ESP_OK;
}

esp_err_t sd_logger_create_test_file(const char *timestamp)
{
    if (!sd_mounted) {
        ESP_LOGE(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    /* Take mutex for SD card access */
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take SD mutex");
        return ESP_ERR_TIMEOUT;
    }

    /* Close existing file if open */
    if (current_file != NULL) {
        fclose(current_file);
        current_file = NULL;
    }

    /* Create filename: TEST_YYYYMMDD_HHMMSS.csv */
    snprintf(current_filename, MAX_PATH_LEN, "%s/TEST_%s.csv", MOUNT_POINT, timestamp);

    /* Open file for writing */
    current_file = fopen(current_filename, "w");
    if (current_file == NULL) {
        ESP_LOGE(TAG, "Failed to create file: %s", current_filename);
        xSemaphoreGive(sd_mutex);
        return ESP_FAIL;
    }

    /* Write CSV header */
    create_csv_header(current_file);
    fflush(current_file);

    sample_count = 0;

    xSemaphoreGive(sd_mutex);

    ESP_LOGI(TAG, "Created test file: %s", current_filename);
    return ESP_OK;
}

esp_err_t sd_logger_write_sample(const adc_sample_t *sample)
{
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (current_file == NULL) {
        ESP_LOGW(TAG, "No file open");
        return ESP_ERR_INVALID_STATE;
    }

    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Take mutex for SD card access */
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        /* Skip sample if can't get mutex (avoid blocking ADC sampling) */
        return ESP_ERR_TIMEOUT;
    }

    /* Write CSV line */
    int result = fprintf(current_file,
        "%" PRIu64 ",%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
        sample->timestamp_us,
        sample->raw_adc[0], sample->raw_adc[1], sample->raw_adc[2],
        sample->raw_adc[3], sample->raw_adc[4], sample->raw_adc[5],
        sample->raw_adc[6], sample->raw_adc[7],
        sample->loadcell_kg, sample->pressure_bar, sample->igniter_v,
        sample->breakwire_v[0], sample->breakwire_v[1],
        sample->breakwire_v[2], sample->breakwire_v[3]
    );

    xSemaphoreGive(sd_mutex);

    if (result < 0) {
        ESP_LOGE(TAG, "Failed to write sample");
        return ESP_FAIL;
    }

    sample_count++;

    /* Flush every 100 samples to minimize data loss */
    if (sample_count % 100 == 0) {
        if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            fflush(current_file);
            xSemaphoreGive(sd_mutex);
        }
    }

    return ESP_OK;
}

esp_err_t sd_logger_write_summary(float duration, float max_thrust,
                                   float total_impulse, float max_pressure)
{
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (current_file == NULL) {
        ESP_LOGW(TAG, "No file open");
        return ESP_ERR_INVALID_STATE;
    }

    /* Take mutex for SD card access */
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take SD mutex");
        return ESP_ERR_TIMEOUT;
    }

    /* Write summary section */
    fprintf(current_file, "\n# SUMMARY\n");
    fprintf(current_file, "# Duration, %.3f, seconds\n", duration);
    fprintf(current_file, "# Max Thrust, %.3f, kg\n", max_thrust);
    fprintf(current_file, "# Total Impulse, %.3f, kg*s\n", total_impulse);
    fprintf(current_file, "# Max Pressure, %.3f, bar\n", max_pressure);
    fprintf(current_file, "# Samples, %lu, count\n", (unsigned long)sample_count);

    /* Flush */
    fflush(current_file);

    xSemaphoreGive(sd_mutex);

    ESP_LOGI(TAG, "Summary written: %.3fs, %.3fkg, %.3fkg*s, %.3fbar",
             duration, max_thrust, total_impulse, max_pressure);

    return ESP_OK;
}

esp_err_t sd_logger_close(void)
{
    if (!sd_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Take mutex for SD card access */
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take SD mutex");
        return ESP_ERR_TIMEOUT;
    }

    /* Close file if open */
    if (current_file != NULL) {
        fflush(current_file);
        fclose(current_file);
        current_file = NULL;
        ESP_LOGI(TAG, "Closed file: %s", current_filename);
    }

    xSemaphoreGive(sd_mutex);

    return ESP_OK;
}

void sd_logging_task(void *pvParameters)
{
    ESP_LOGI(TAG, "SD logging task started");

    while (1) {
        /* This task handles queue-based logging */
        /* For now, samples are written directly in sd_logger_write_sample */
        /* This task can be used for buffering and asynchronous writes */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* Utility function to get current filename */
const char *sd_logger_get_filename(void)
{
    if (current_file == NULL) {
        return NULL;
    }
    return current_filename;
}

/* Utility function to get sample count */
uint32_t sd_logger_get_sample_count(void)
{
    return sample_count;
}

/*******************************************************************************
 * Internal Helper Functions
 ******************************************************************************/

static void create_csv_header(FILE *f)
{
    if (f == NULL) {
        return;
    }

    /* Write CSV header */
    fprintf(f, "# Static Test Stand Controller - Data Log\n");
    fprintf(f, "# Timestamp_us,ADC0,ADC1,ADC2,ADC3,ADC4,ADC5,ADC6,ADC7,");
    fprintf(f, "LoadCell_kg,Pressure_bar,Igniter_V,");
    fprintf(f, "BreakWire1_V,BreakWire2_V,BreakWire3_V,BreakWire4_V\n");
}

#endif /* BUILD_TARGET_BASE */
