/**
 * @file mp_safety.c
 * @brief Safety Layer implementation for Zephyr + MicroPython platform.
 */

#include "mp_safety.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/watchdog.h>
#include <string.h>

/* MicroPython headers */
#include "py/mpconfig.h"
#include "py/gc.h"

/* ------------------------------------------------------------------------- */
/* 1. Watchdog Management                                                    */
/* ------------------------------------------------------------------------- */

static const struct device *s_wdt_dev = NULL;
static int s_wdt_channel_id = -1;

int mp_safety_wdt_init(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_wdt)
    s_wdt_dev = DEVICE_DT_GET_ANY(nordic_nrf_wdt);
#else
    s_wdt_dev = NULL;
#endif

    if (s_wdt_dev == NULL || !device_is_ready(s_wdt_dev)) {
        printk("[MP_SAFETY] WDT hardware device absent or not ready on this platform.\n");
        return -ENODEV;
    }

    struct wdt_timeout_cfg cfg = {
        .window = {
            .min = 0U,
            .max = MP_SAFETY_WDT_TIMEOUT_MS,
        },
        .callback = NULL,
        .flags = WDT_FLAG_RESET_SOC,
    };

    s_wdt_channel_id = wdt_install_timeout(s_wdt_dev, &cfg);
    if (s_wdt_channel_id < 0) {
        printk("[MP_SAFETY] Failed to install WDT timeout: %d\n", s_wdt_channel_id);
        return s_wdt_channel_id;
    }

    int err = wdt_setup(s_wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (err < 0) {
        printk("[MP_SAFETY] Failed to configure WDT: %d\n", err);
        return err;
    }

    printk("[MP_SAFETY] Hardware Watchdog active (%u ms timeout, channel %d).\n",
           MP_SAFETY_WDT_TIMEOUT_MS, s_wdt_channel_id);
    return 0;
}

void mp_safety_wdt_kick(void)
{
    if (s_wdt_dev != NULL && s_wdt_channel_id >= 0) {
        wdt_feed(s_wdt_dev, s_wdt_channel_id);
    }
}

/* ------------------------------------------------------------------------- */
/* 2. Script Size & Content Validation                                       */
/* ------------------------------------------------------------------------- */

bool mp_safety_script_allowed(const char *src)
{
    if (src == NULL) {
        return false;
    }
    size_t len = strlen(src);
    if (len == 0u || len > MP_SAFETY_MAX_SCRIPT_SIZE) {
        printk("[MP_SAFETY] Script size check failed (%zu bytes, max %u).\n",
               len, MP_SAFETY_MAX_SCRIPT_SIZE);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* 3. Module Import Security Whitelist                                       */
/* ------------------------------------------------------------------------- */

static const char * const s_whitelisted_modules[] = {
    "builtins", "sys", "math", "struct", "ustruct",
    "utime", "time", "uio", "io", "ujson", "json",
    "ure", "re", "uhashlib", "hashlib", "ubinascii",
    "binascii", "machine", "zephyr", "hw_hal", NULL
};

bool mp_safety_module_allowed(const char *module_name)
{
    if (module_name == NULL) {
        return false;
    }
    for (size_t i = 0; s_whitelisted_modules[i] != NULL; i++) {
        if (strcmp(module_name, s_whitelisted_modules[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* 4. Heap Guard & Memory Utilization Monitor                                */
/* ------------------------------------------------------------------------- */

void mp_safety_heap_check(void)
{
    gc_info_t info;
    gc_info(&info);

    size_t pct = (info.total > 0u) ? ((info.used * 100u) / info.total) : 0u;

    if (pct >= MP_SAFETY_HEAP_ABORT_PCT) {
        printk("[MP_SAFETY] CRITICAL: GC Heap at %zu%% (%zu/%zu bytes). Rebooting...\n",
               pct, info.used, info.total);
#if defined(CONFIG_REBOOT)
        sys_reboot(SYS_REBOOT_COLD);
#else
        k_panic();
#endif
    } else if (pct >= MP_SAFETY_HEAP_WARN_PCT) {
        printk("[MP_SAFETY] WARNING: GC Heap utilization elevated: %zu%% (%zu/%zu bytes).\n",
               pct, info.used, info.total);
    } else {
        printk("[MP_SAFETY] Heap check: %zu%% used (%zu/%zu bytes, %zu free).\n",
               pct, info.used, info.total, info.free);
    }
}

/* ------------------------------------------------------------------------- */
/* 5. Uncaught NLR Escape Handler (REQ-04)                                   */
/* ------------------------------------------------------------------------- */

void NORETURN nlr_jump_fail(void *val)
{
    (void)val;
    printk("[MP_SAFETY] FATAL: Uncaught MicroPython NLR exception escaped! Cold rebooting...\n");
#if defined(CONFIG_REBOOT)
    sys_reboot(SYS_REBOOT_COLD);
#else
    k_panic();
#endif
    CODE_UNREACHABLE;
}
