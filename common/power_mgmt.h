/**
 * @file power_mgmt.h
 * @brief Low-Power Management, GPIO Leakage Prevention & Thread-Safe VM Mutex.
 *
 * Implements Person 2's power management and concurrency deliverables:
 *  1. Thread-safety mutex ensuring re-entrant protection for MicroPython VM execution.
 *  2. GPIO low-power state management (GPIO_DISCONNECTED) to eliminate quiescent leakage.
 *  3. Event-driven interrupt UART reception to replace polling loops and enable tickless idle.
 */

#ifndef POWER_MGMT_H_
#define POWER_MGMT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include "hw_hal.h"

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================= */
/*                   1. THREAD SAFETY MUTEX FOR VM EXECUTION                 */
/* ========================================================================= */

/**
 * @brief Acquire the MicroPython VM execution mutex for thread safety.
 *
 * Prevents concurrent re-entry into the MicroPython engine across multiple
 * Zephyr threads or asynchronous task contexts.
 *
 * @param timeout Maximum duration to wait before returning error.
 * @return 0 on success, -EAGAIN on timeout, or negative errno.
 */
int pm_vm_lock(k_timeout_t timeout);

/**
 * @brief Release the MicroPython VM execution mutex.
 */
void pm_vm_unlock(void);

/**
 * @brief Query if the VM execution mutex is currently acquired.
 *
 * @return true if locked, false otherwise.
 */
bool pm_vm_is_locked(void);

/* ========================================================================= */
/*               2. GPIO LOW-POWER STATE (LEAKAGE PREVENTION)                */
/* ========================================================================= */

/**
 * @brief Place a single GPIO pin into low-power (disconnected) state.
 *
 * Reconfigures the pin with GPIO_DISCONNECTED, disabling both the input buffer
 * and output driver to eliminate parasitic leakage current across floating lines
 * or internal pull-up / pull-down resistors.
 *
 * @param port GPIO controller device.
 * @param pin  Pin number.
 * @return 0 on success, negative errno on failure.
 */
int pm_gpio_set_low_power(const struct device *port, gpio_pin_t pin);

/**
 * @brief Place a HAL GPIO descriptor into low-power disconnected state.
 *
 * @param hal_pin Pointer to hal_gpio_pin_t descriptor.
 * @return 0 on success, negative errno on failure.
 */
int pm_gpio_set_hal_pin_low_power(const hal_gpio_pin_t *hal_pin);

/**
 * @brief Helper to disconnect all board demo/LED pins after script execution.
 *
 * Automatically called when MicroPython finishes executing a script to return
 * peripheral GPIO rails to minimum current draw.
 *
 * @return 0 on success.
 */
int pm_gpio_restore_all_low_power(void);

/* ========================================================================= */
/*              3. EVENT-DRIVEN UART RECEPTION (TICKLESS IDLE)               */
/* ========================================================================= */

typedef void (*pm_uart_rx_cb_t)(uint8_t byte, void *user_data);

/**
 * @brief Enable interrupt-driven (event-driven) UART reception.
 *
 * Replaces CPU-intensive polling loops (e.g. k_msleep/console_getchar) with
 * hardware RX interrupts, allowing Zephyr's power subsystem (CONFIG_PM=y)
 * to keep the Cortex-M33 in low-power WFI / Tickless Idle sleep.
 *
 * @param uart_dev  Pointer to UART device.
 * @param cb        Callback invoked in ISR context when a byte is received.
 * @param user_data User-provided context pointer.
 * @return 0 on success, negative errno on failure.
 */
int pm_uart_enable_rx_event_driven(const struct device *uart_dev, pm_uart_rx_cb_t cb, void *user_data);

/**
 * @brief Disable event-driven UART RX interrupts.
 *
 * @param uart_dev Pointer to UART device.
 * @return 0 on success, negative errno on failure.
 */
int pm_uart_disable_rx_event_driven(const struct device *uart_dev);

/**
 * @brief Enter low-power idle sleep until next interrupt.
 *
 * Yields the CPU in a low-power state (Wait-For-Interrupt) rather than spinning.
 *
 * @param duration Duration to sleep (or K_FOREVER).
 */
void pm_enter_low_power_idle(k_timeout_t duration);

/**
 * @brief Self-test and validation stub for Person 2's power management module.
 */
void pm_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MGMT_H_ */
