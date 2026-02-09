/*******************************************************************************
 * REMOTE Unit - SSD1306 OLED Display Driver
 *
 * Manages the 128x64 OLED display via I2C.
 * See FSD Section 5.1 for display layout specifications.
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_REMOTE

#include "display_ssd1306.h"
#include "shared_queues.h"
#include "esp_now_protocol.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "display";

#define SSD1306_ADDR            0x3C
#define SSD1306_CMD_PREFIX      0x00
#define SSD1306_DATA_PREFIX     0x40
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ     400000
#define DISPLAY_REFRESH_MS      200
#define CHAR_WIDTH              6  /* 5px + 1px spacing */
#define CHAR_HEIGHT             8
#define CHARS_PER_LINE          (DISPLAY_WIDTH / CHAR_WIDTH)  /* 21 chars */
#define PAGE_COUNT              (DISPLAY_HEIGHT / 8)          /* 8 pages */

/* Framebuffer: 128 x 8 pages = 1024 bytes */
static uint8_t s_framebuf[DISPLAY_WIDTH * PAGE_COUNT];

/* Display state */
static display_params_t s_params = {0};
static bool s_initialized = false;

/*******************************************************************************
 * Minimal 5x7 font for ASCII 32-126
 * Each character is 5 bytes wide (columns), each byte is a vertical column
 * Bit 0 = top pixel, Bit 6 = bottom pixel
 ******************************************************************************/

static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32 space */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 34 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 37 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 38 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 39 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41 ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 42 * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43 + */
    {0x00,0x50,0x30,0x00,0x00}, /* 44 , */
    {0x08,0x08,0x08,0x08,0x08}, /* 45 - */
    {0x00,0x60,0x60,0x00,0x00}, /* 46 . */
    {0x20,0x10,0x08,0x04,0x02}, /* 47 / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 50 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 53 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 55 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 56 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 58 : */
    {0x00,0x56,0x36,0x00,0x00}, /* 59 ; */
    {0x00,0x08,0x14,0x22,0x41}, /* 60 < */
    {0x14,0x14,0x14,0x14,0x14}, /* 61 = */
    {0x41,0x22,0x14,0x08,0x00}, /* 62 > */
    {0x02,0x01,0x51,0x09,0x06}, /* 63 ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69 E */
    {0x7F,0x09,0x09,0x01,0x01}, /* 70 F */
    {0x3E,0x41,0x41,0x51,0x32}, /* 71 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74 J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75 K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76 L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 77 M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78 N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79 O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 83 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86 V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 87 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 88 X */
    {0x03,0x04,0x78,0x04,0x03}, /* 89 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 90 Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* 91 [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 92 backslash */
    {0x41,0x41,0x7F,0x00,0x00}, /* 93 ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 94 ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 95 _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 96 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 97 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 99 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 101 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102 f */
    {0x08,0x14,0x54,0x54,0x3C}, /* 103 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106 j */
    {0x00,0x7F,0x10,0x28,0x44}, /* 107 k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108 l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109 m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110 n */
    {0x38,0x44,0x44,0x44,0x38}, /* 111 o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 115 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 z */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 | */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 } */
    {0x08,0x08,0x2A,0x1C,0x08}, /* 126 ~ */
};

/*******************************************************************************
 * I2C / SSD1306 Low-Level
 ******************************************************************************/

static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { SSD1306_CMD_PREFIX, cmd };
    return i2c_master_write_to_device(I2C_MASTER_NUM, SSD1306_ADDR,
                                       buf, sizeof(buf),
                                       pdMS_TO_TICKS(100));
}

static esp_err_t ssd1306_write_data(const uint8_t *data, size_t len)
{
    uint8_t *buf = malloc(len + 1);
    if (buf == NULL) return ESP_ERR_NO_MEM;
    buf[0] = SSD1306_DATA_PREFIX;
    memcpy(buf + 1, data, len);
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, SSD1306_ADDR,
                                                buf, len + 1,
                                                pdMS_TO_TICKS(100));
    free(buf);
    return ret;
}

static void ssd1306_flush(void)
{
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    /* Set column and page address to cover full display */
    ssd1306_write_cmd(0x21); /* Column address */
    ssd1306_write_cmd(0);
    ssd1306_write_cmd(DISPLAY_WIDTH - 1);
    ssd1306_write_cmd(0x22); /* Page address */
    ssd1306_write_cmd(0);
    ssd1306_write_cmd(PAGE_COUNT - 1);

    ssd1306_write_data(s_framebuf, sizeof(s_framebuf));

    xSemaphoreGive(i2c_mutex);
}

/*******************************************************************************
 * Framebuffer Drawing
 ******************************************************************************/

static void fb_clear(void)
{
    memset(s_framebuf, 0, sizeof(s_framebuf));
}

static void fb_draw_char(int x, int page, char c)
{
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;

    for (int col = 0; col < 5; col++) {
        int px = x + col;
        if (px >= 0 && px < DISPLAY_WIDTH) {
            s_framebuf[page * DISPLAY_WIDTH + px] = font5x7[idx][col];
        }
    }
}

static void fb_draw_string(int x, int page, const char *str)
{
    for (int i = 0; str[i] != '\0' && (x + i * CHAR_WIDTH) < DISPLAY_WIDTH; i++) {
        fb_draw_char(x + i * CHAR_WIDTH, page, str[i]);
    }
}

/*******************************************************************************
 * Public API
 ******************************************************************************/

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing SSD1306 display");

    /* I2C master config */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));

    /* SSD1306 initialization sequence */
    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_FAIL;
    }

    ssd1306_write_cmd(0xAE); /* Display OFF */
    ssd1306_write_cmd(0xD5); ssd1306_write_cmd(0x80); /* Clock div */
    ssd1306_write_cmd(0xA8); ssd1306_write_cmd(0x3F); /* Mux ratio 64 */
    ssd1306_write_cmd(0xD3); ssd1306_write_cmd(0x00); /* Display offset 0 */
    ssd1306_write_cmd(0x40); /* Start line 0 */
    ssd1306_write_cmd(0x8D); ssd1306_write_cmd(0x14); /* Charge pump enable */
    ssd1306_write_cmd(0x20); ssd1306_write_cmd(0x00); /* Horizontal addressing */
    ssd1306_write_cmd(0xA1); /* Segment remap */
    ssd1306_write_cmd(0xC8); /* COM scan direction */
    ssd1306_write_cmd(0xDA); ssd1306_write_cmd(0x12); /* COM pins */
    ssd1306_write_cmd(0x81); ssd1306_write_cmd(0xCF); /* Contrast */
    ssd1306_write_cmd(0xD9); ssd1306_write_cmd(0xF1); /* Pre-charge */
    ssd1306_write_cmd(0xDB); ssd1306_write_cmd(0x40); /* VCOMH deselect */
    ssd1306_write_cmd(0xA4); /* Resume from RAM */
    ssd1306_write_cmd(0xA6); /* Normal display (not inverted) */
    ssd1306_write_cmd(0xAF); /* Display ON */

    xSemaphoreGive(i2c_mutex);

    fb_clear();
    ssd1306_flush();
    s_initialized = true;

    ESP_LOGI(TAG, "SSD1306 display initialized");
    return ESP_OK;
}

esp_err_t display_clear(void)
{
    fb_clear();
    memset(&s_params, 0, sizeof(s_params));
    ssd1306_flush();
    return ESP_OK;
}

esp_err_t display_update(const display_params_t *params)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    memcpy(&s_params, params, sizeof(display_params_t));

    fb_clear();

    /* Page 0: Status bar "ST:<state> F:<fails>" */
    char status_line[22];
    snprintf(status_line, sizeof(status_line), "ST:%-6s F:%u",
             s_params.base_state, s_params.tx_rx_fails);
    fb_draw_string(0, 0, status_line);

    /* Horizontal line separator at page 1 top */
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        s_framebuf[1 * DISPLAY_WIDTH + x] = 0x01;
    }

    /* Pages 2-6: Log lines (5 lines) */
    for (int i = 0; i < LOG_LINE_COUNT && i < 5; i++) {
        fb_draw_string(0, 2 + i, s_params.log_lines[i]);
    }

    ssd1306_flush();
    return ESP_OK;
}

esp_err_t display_add_log_line(const char *line)
{
    /* Scroll lines up */
    for (int i = 0; i < LOG_LINE_COUNT - 1; i++) {
        memcpy(s_params.log_lines[i], s_params.log_lines[i + 1],
               sizeof(s_params.log_lines[0]));
    }
    /* Add new line at bottom */
    strncpy(s_params.log_lines[LOG_LINE_COUNT - 1], line,
            sizeof(s_params.log_lines[0]) - 1);
    s_params.log_lines[LOG_LINE_COUNT - 1][sizeof(s_params.log_lines[0]) - 1] = '\0';

    return display_update(&s_params);
}

esp_err_t display_show_sensor_value(const char *label, float value)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    fb_clear();

    /* Center label on page 2 */
    int label_len = strlen(label);
    int x_offset = (DISPLAY_WIDTH - label_len * CHAR_WIDTH) / 2;
    if (x_offset < 0) x_offset = 0;
    fb_draw_string(x_offset, 2, label);

    /* Center value on page 4 */
    char val_str[22];
    snprintf(val_str, sizeof(val_str), "%.2f", value);
    int val_len = strlen(val_str);
    x_offset = (DISPLAY_WIDTH - val_len * CHAR_WIDTH) / 2;
    if (x_offset < 0) x_offset = 0;
    fb_draw_string(x_offset, 4, val_str);

    ssd1306_flush();
    return ESP_OK;
}

const char *display_get_base_state(void)
{
    return s_params.base_state;
}

void display_update_task(void *pvParameters)
{
    espnow_packet_t cmd_pkt;
    TickType_t last_refresh = 0;

    ESP_LOGI(TAG, "Display update task started");

    for (;;) {
        /* Check for display commands with short timeout for periodic refresh */
        if (xQueueReceive(display_cmd_queue, &cmd_pkt,
                          pdMS_TO_TICKS(DISPLAY_REFRESH_MS)) == pdTRUE) {
            /* State name lookup table */
            static const char *state_names[] = {
                "INIT","IDLE","ARMED","START","IGNIT",
                "RUN","END","HALT","CHKIG","CHKBR",
                "CALLC","CALPR","WELCM"
            };

            switch (cmd_pkt.command) {
                case CMD_DISPLAY_CLEAR:
                    display_clear();
                    break;
                case CMD_DISPLAY_LOG_LINE:
                    display_add_log_line(cmd_pkt.message);
                    break;
                case CMD_DISPLAY_SENSOR: {
                    float value = (float)cmd_pkt.data / 100.0f;
                    display_show_sensor_value(cmd_pkt.message, value);
                    break;
                }
                default:
                    /* LED commands or other commands - just update state */
                    break;
            }

            /* Update state from any packet that has valid base_state */
            if (cmd_pkt.base_state < 13) {
                strncpy(s_params.base_state, state_names[cmd_pkt.base_state],
                        sizeof(s_params.base_state) - 1);
                s_params.base_state[sizeof(s_params.base_state) - 1] = '\0';
            }
        }

        /* Periodic refresh */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_refresh) >= pdMS_TO_TICKS(DISPLAY_REFRESH_MS)) {
            display_update(&s_params);
            last_refresh = now;
        }
    }
}

#endif /* BUILD_TARGET_REMOTE */
