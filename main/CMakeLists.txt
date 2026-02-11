# Main component CMakeLists.txt
# Conditionally includes BASE or REMOTE source files

# Read BUILD_TARGET from environment if not set (needed during component discovery)
if(NOT DEFINED BUILD_TARGET AND DEFINED ENV{BUILD_TARGET})
    set(BUILD_TARGET "$ENV{BUILD_TARGET}")
endif()

# Check if we're building the settings writer utility
if(DEFINED ENV{SETTINGS_WRITER})
    set(SETTINGS_WRITER "$ENV{SETTINGS_WRITER}")
else()
    set(SETTINGS_WRITER "0")
endif()

set(COMMON_SRCS
    "common/esp_now_protocol.c"
    "common/rgb_led.c"
    "common/buzzer.c"
    "common/ping_monitor.c"
    "common/safety.c"
    "common/shared_queues.c"
    "common/test_protocol.c"
)

if(BUILD_TARGET STREQUAL "BASE")
    if(SETTINGS_WRITER STREQUAL "1")
        # Build settings writer utility (minimal firmware)
        set(TARGET_SRCS
            "base/settings_writer_main.c"
        )
        add_compile_definitions(PRIVATE SETTINGS_WRITER=1)
    else()
        # Build normal BASE firmware
        set(TARGET_SRCS
            "base/base_main.c"
            "base/state_machine.c"
            "base/adc_as1256.c"
            "base/sd_logger.c"
            "base/rtc_ds1307.c"
            "base/settings.c"
            "base/igniter.c"
        )
    endif()
elseif(BUILD_TARGET STREQUAL "REMOTE")
    set(TARGET_SRCS
        "remote/remote_main.c"
        "remote/display_ssd1306.c"
        "remote/input_handler.c"
        "remote/battery_monitor.c"
        "remote/button_led.c"
        "remote/cmd_dispatch.c"
    )
else()
    message(FATAL_ERROR "BUILD_TARGET must be BASE or REMOTE, got: ${BUILD_TARGET}")
endif()

idf_component_register(
    SRCS "main.c" ${COMMON_SRCS} ${TARGET_SRCS}
    INCLUDE_DIRS "." "base" "remote" "common"
    REQUIRES driver esp_wifi nvs_flash fatfs spi_flash led_strip esp_timer
)
