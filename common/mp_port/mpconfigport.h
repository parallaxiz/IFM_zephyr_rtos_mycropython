#ifndef MPCONFIGPORT_H
#define MPCONFIGPORT_H

// Base MicroPython ROM feature level definitions (matching py/mpconfig.h)
#ifndef MICROPY_CONFIG_ROM_LEVEL_MINIMUM
#define MICROPY_CONFIG_ROM_LEVEL_MINIMUM (0)
#define MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES (10)
#define MICROPY_CONFIG_ROM_LEVEL_BASIC_FEATURES (20)
#define MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES (30)
#define MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES (40)
#define MICROPY_CONFIG_ROM_LEVEL_EVERYTHING (50)
#endif

// Ensure a Zephyr MicroPython ROM level configuration flag is defined if none is provided via Kconfig
#if !defined(CONFIG_MICROPY_CONFIG_ROM_LEVEL_MINIMUM) && \
    !defined(CONFIG_MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES) && \
    !defined(CONFIG_MICROPY_CONFIG_ROM_LEVEL_BASIC_FEATURES) && \
    !defined(CONFIG_MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES) && \
    !defined(CONFIG_MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES) && \
    !defined(CONFIG_MICROPY_CONFIG_ROM_LEVEL_EVERYTHING)
#if defined(MICROPY_CONFIG_ROM_LEVEL)
#if MICROPY_CONFIG_ROM_LEVEL == MICROPY_CONFIG_ROM_LEVEL_MINIMUM
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_MINIMUM (1)
#elif MICROPY_CONFIG_ROM_LEVEL == MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_CORE_FEATURES (1)
#elif MICROPY_CONFIG_ROM_LEVEL == MICROPY_CONFIG_ROM_LEVEL_BASIC_FEATURES
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_BASIC_FEATURES (1)
#elif MICROPY_CONFIG_ROM_LEVEL == MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES (1)
#elif MICROPY_CONFIG_ROM_LEVEL == MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES (1)
#elif MICROPY_CONFIG_ROM_LEVEL == MICROPY_CONFIG_ROM_LEVEL_EVERYTHING
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_EVERYTHING (1)
#else
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES (1)
#endif
#else
#define CONFIG_MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES (1)
#endif
#endif

#include "../micropython/ports/zephyr/mpconfigport.h"

#define MICROPY_PY_SYS_PATH (1)


/* ============================================================
 * Module Security Whitelist
 * Explicitly disable modules not needed for embedded hardware
 * scripts. Only machine, time, math and our hw_hal are allowed.
 * ============================================================ */

/* Block filesystem and OS access from Python scripts */
#undef  MICROPY_PY_OS
#define MICROPY_PY_OS               (0)
#undef  MICROPY_PY_OS_URANDOM
#define MICROPY_PY_OS_URANDOM       (0)

/* Block socket / network access */
#ifndef MICROPY_PY_SOCKET
#define MICROPY_PY_SOCKET           (0)
#endif

/* Block raw ctypes-style memory access */
#ifndef MICROPY_PY_UCTYPES
#define MICROPY_PY_UCTYPES          (0)
#endif

/* Block uselect / uasyncio (not needed, prevents hidden blocking) */
#ifndef MICROPY_PY_USELECT
#define MICROPY_PY_USELECT          (0)
#endif

/* Allow: machine, time, math, gc */
/* These are enabled by the upstream Zephyr mpconfigport.h and kept as-is */

#endif /* MPCONFIGPORT_H */
