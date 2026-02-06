/*******************************************************************************
 * StaticTeststandController - Main Entry Point
 *
 * Dispatches to base_main() or remote_main() based on BUILD_TARGET.
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
#ifdef BUILD_TARGET_BASE
    base_main();
#elif defined(BUILD_TARGET_REMOTE)
    remote_main();
#else
    #error "No valid BUILD_TARGET defined"
#endif
}
