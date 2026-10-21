/**
 * @file fault_log.h
 * @brief Persistent Fault Logger and Boot Counter using Zephyr NVS/Flash.
 *
 * Implements Person 2's deliverables:
 *  1. Boot counter stored in non-volatile flash to track total reboots.
 *  2. Consecutive crash streak tracking to detect reboot/crash loops.
 *  3. Circular persistent fault log records surviving warm/cold reboots.
 */

#ifndef FAULT_LOG_H_
#define FAULT_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FAULT_LOG_MSG_MAX_LEN 48
#define FAULT_LOG_MAX_ENTRIES 16
#define FAULT_LOG_CRASH_LOOP_THRESHOLD 3

typedef enum {
    FAULT_TYPE_OK = 0,
    FAULT_TYPE_SYSTEM_BOOT = 1,
    FAULT_TYPE_NLR_PANIC = 2,
    FAULT_TYPE_WATCHDOG_RESET = 3,
    FAULT_TYPE_OOM = 4,
    FAULT_TYPE_PYTHON_EXCEPTION = 5,
    FAULT_TYPE_CRASH_LOOP_DETECTED = 6,
    FAULT_TYPE_HARDWARE_FAULT = 7,
    FAULT_TYPE_USER = 8,
} fault_type_t;

/**
 * @brief Persistent fault record structure stored in Flash NVS.
 */
typedef struct {
    uint32_t timestamp_ms;
    uint16_t fault_type;
    uint16_t code;
    char message[FAULT_LOG_MSG_MAX_LEN];
} fault_record_t;

/**
 * @brief Initialize the persistent fault logging subsystem and NVS filesystem.
 *
 * @return 0 on success, or negative errno on failure.
 */
int fault_log_init(void);

/**
 * @brief Increment the persistent boot counter and check for crash loops.
 *
 * Checks whether consecutive reboots occurred without a successful execution
 * checkpoint. If consecutive crashes exceed FAULT_LOG_CRASH_LOOP_THRESHOLD,
 * out_crash_loop is set to true.
 *
 * @param out_boot_count   Pointer to store the updated total boot count.
 * @param out_crash_loop   Pointer to store true if a crash loop is detected.
 * @return 0 on success, or negative errno on failure.
 */
int fault_log_boot_counter_increment(uint32_t *out_boot_count, bool *out_crash_loop);

/**
 * @brief Retrieve the current total boot count.
 *
 * @param out_boot_count Pointer to store current boot count.
 * @return 0 on success, or negative errno on failure.
 */
int fault_log_get_boot_count(uint32_t *out_boot_count);

/**
 * @brief Reset the consecutive crash streak counter.
 *
 * Must be called when system boots stably and executes code successfully,
 * proving that the system is not stuck in a crash loop.
 *
 * @return 0 on success, or negative errno on failure.
 */
int fault_log_clear_crash_streak(void);

/**
 * @brief Record a fault event into persistent Flash storage.
 *
 * @param type Fault category identifier.
 * @param code Specific error or fault status code.
 * @param msg  Descriptive message (truncated to FAULT_LOG_MSG_MAX_LEN - 1).
 * @return 0 on success, or negative errno on failure.
 */
int fault_log_record(fault_type_t type, uint16_t code, const char *msg);

/**
 * @brief Print all stored persistent fault records over the console/UART.
 *
 * @return Number of records printed, or negative errno on failure.
 */
int fault_log_dump(void);

/**
 * @brief Erase all recorded faults from persistent storage (resets fault log).
 *
 * @return 0 on success, or negative errno on failure.
 */
int fault_log_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif /* FAULT_LOG_H_ */
