/**
 * @file mp_safety.h
 * @brief Safety Layer: Watchdog, Heap Guard, Script Size Limits & Module Whitelist.
 *
 * Implements requirements REQ-04 through REQ-09:
 *  - Uncaught NLR escape triggers cold system reboot
 *  - Script size enforcement before VM execution
 *  - MicroPython module import whitelist checking
 *  - Heap guard monitoring with warning (80%) and reboot (95%)
 *  - Hardware watchdog management with 5000ms window
 */

#ifndef MP_SAFETY_H_
#define MP_SAFETY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MP_SAFETY_MAX_SCRIPT_SIZE  4096U
#define MP_SAFETY_HEAP_WARN_PCT    80U
#define MP_SAFETY_HEAP_ABORT_PCT   95U
#define MP_SAFETY_WDT_TIMEOUT_MS   5000U

/**
 * @brief Initialise the hardware watchdog timer.
 *
 * @return 0 on success, -ENODEV if not available (e.g. native_sim), or negative errno.
 */
int mp_safety_wdt_init(void);

/**
 * @brief Kick / feed the hardware watchdog timer.
 */
void mp_safety_wdt_kick(void);

/**
 * @brief Validate that a Python script conforms to length and safety constraints.
 *
 * @param src Null-terminated Python script string.
 * @return true if accepted, false if rejected.
 */
bool mp_safety_script_allowed(const char *src);

/**
 * @brief Check if an imported module is permitted by the security whitelist.
 *
 * @param module_name Name of the module being imported.
 * @return true if whitelisted, false otherwise.
 */
bool mp_safety_module_allowed(const char *module_name);

/**
 * @brief Inspect MicroPython GC heap utilisation.
 *
 * Emits a warning if utilisation exceeds 80%, and triggers a cold reboot
 * if utilisation exceeds 95% to prevent memory exhaustion / silent corruption.
 */
void mp_safety_heap_check(void);

#ifdef __cplusplus
}
#endif

#endif /* MP_SAFETY_H_ */
