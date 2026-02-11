/*******************************************************************************
 * StaticTeststandController - Main Entry Point
 *
 * Dispatches to:
 * - settings_writer_main() if BUILD_TARGET=BASE and SETTINGS_WRITER=1
 * - base_main() if BUILD_TARGET=BASE (normal firmware)
 * - remote_main() if BUILD_TARGET=REMOTE
 ******************************************************************************/

#include "config.h"

#ifdef BUILD_TARGET_BASE
extern void base_main(void);
#endif

#ifdef BUILD_TARGET_REMOTE
extern void remote_main(void);
#endif

void app_main(void)
{
#if SETTINGS_WRITER == 1
    extern void settings_writer_main(void);
    settings_writer_main();
#elif defined(BUILD_TARGET_BASE)
    base_main();
#elif defined(BUILD_TARGET_REMOTE)
    remote_main();
#else
    #error "No valid BUILD_TARGET defined"
#endif
}
