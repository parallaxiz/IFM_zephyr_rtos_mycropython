/**
 * @file hw_hal.h
 * @brief Lightweight Hardware Abstraction Layer (HAL) for Zephyr RTOS.
 *
 * Provides modular, device-independent abstractions for:
 *  1. GPIO: Pin initialization, read, write, and toggle.
 *  2. UART: Polling-based byte and buffer send/receive with timeout handling.
 *  3. TIMER: Periodic and one-shot hardware/kernel timers and delay wrappers.
 *  4. LOGGING: Custom timestamped logging wrapper with severity levels.
 */

#ifndef HW_HAL_H_
#define HW_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

/* ========================================================================= */
/*                          4. USER-DEFINED LOGGING                          */
/* ========================================================================= */

typedef enum {
    HAL_LOG_LEVEL_NONE = 0,
    HAL_LOG_LEVEL_ERROR,
    HAL_LOG_LEVEL_WARN,
    HAL_LOG_LEVEL_INFO,
    HAL_LOG_LEVEL_DEBUG,
} hal_log_level_t;

/**
 * @brief Set the minimum runtime log level for HAL logging.
 *
 * @param level Desired threshold level (HAL_LOG_LEVEL_*).
 */
void hal_log_set_level(hal_log_level_t level);

/**
 * @brief Get the current runtime log level.
 *
 * @return Current log level.
 */
hal_log_level_t hal_log_get_level(void);

/**
 * @brief Core logging function with timestamp and severity formatting.
 *
 * @param level Severity level of the message.
 * @param tag   Module or subsystem identifier tag (e.g., "MAIN", "GPIO").
 * @param fmt   Printf-style format string.
 * @param ...   Format arguments.
 */
void hal_log(hal_log_level_t level, const char *tag, const char *fmt, ...);

/* Convenience logging macros with compile-time tag and severity */
#define HAL_LOG_ERROR(tag, fmt, ...) hal_log(HAL_LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_WARN(tag, fmt, ...)  hal_log(HAL_LOG_LEVEL_WARN,  tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_INFO(tag, fmt, ...)  hal_log(HAL_LOG_LEVEL_INFO,  tag, fmt, ##__VA_ARGS__)
#define HAL_LOG_DEBUG(tag, fmt, ...) hal_log(HAL_LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)

/* ========================================================================= */
/*                            1. GPIO ABSTRACTION                            */
/* ========================================================================= */

typedef enum {
    HAL_GPIO_DIR_INPUT = 0,
    HAL_GPIO_DIR_OUTPUT,
} hal_gpio_dir_t;

typedef enum {
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP,
    HAL_GPIO_PULL_DOWN,
} hal_gpio_pull_t;

/**
 * @brief GPIO pin descriptor structure.
 */
typedef struct {
    const struct device *port;  /**< Pointer to GPIO device controller (e.g., DEVICE_DT_GET(...)) */
    gpio_pin_t pin;             /**< Pin number on the port */
    gpio_flags_t extra_flags;   /**< Optional extra flags (e.g., GPIO_ACTIVE_LOW) */
} hal_gpio_pin_t;

/**
 * @brief Helper initializer for GPIO pin descriptor.
 */
#define HAL_GPIO_PIN_INIT(_port, _pin) \
    { .port = (_port), .pin = (_pin), .extra_flags = 0 }

#define HAL_GPIO_PIN_INIT_FLAGS(_port, _pin, _flags) \
    { .port = (_port), .pin = (_pin), .extra_flags = (_flags) }

/**
 * @brief Configure and initialize a GPIO pin.
 *
 * @param pin  Pointer to GPIO pin descriptor.
 * @param dir  Pin direction (Input or Output).
 * @param pull Pull configuration (None, Pull-Up, or Pull-Down).
 * @return 0 on success, negative errno on failure.
 */
int hal_gpio_init(const hal_gpio_pin_t *pin, hal_gpio_dir_t dir, hal_gpio_pull_t pull);

/**
 * @brief Convenience helper to initialize a pin as output with initial state.
 *
 * @param pin           Pointer to GPIO pin descriptor.
 * @param initial_state Initial state (0 for low, 1 for high).
 * @return 0 on success, negative errno on failure.
 */
int hal_gpio_init_output(const hal_gpio_pin_t *pin, int initial_state);

/**
 * @brief Convenience helper to initialize a pin as input.
 *
 * @param pin  Pointer to GPIO pin descriptor.
 * @param pull Pull configuration (None, Pull-Up, Pull-Down).
 * @return 0 on success, negative errno on failure.
 */
int hal_gpio_init_input(const hal_gpio_pin_t *pin, hal_gpio_pull_t pull);

/**
 * @brief Set the output state of a GPIO pin.
 *
 * @param pin   Pointer to GPIO pin descriptor.
 * @param state State to set: false (low) or true (high).
 * @return 0 on success, negative errno on failure.
 */
int hal_gpio_set(const hal_gpio_pin_t *pin, bool state);

/**
 * @brief Toggle the state of an output GPIO pin.
 *
 * @param pin Pointer to GPIO pin descriptor.
 * @return 0 on success, negative errno on failure.
 */
int hal_gpio_toggle(const hal_gpio_pin_t *pin);

/**
 * @brief Read the current digital state of a GPIO pin.
 *
 * @param pin Pointer to GPIO pin descriptor.
 * @return 0 (low), 1 (high), or negative errno on failure.
 */
int hal_gpio_get(const hal_gpio_pin_t *pin);

/* ========================================================================= */
/*                            2. UART ABSTRACTION                            */
/* ========================================================================= */

/**
 * @brief Initialize and verify readiness of a UART device.
 *
 * @param uart_dev Pointer to UART device.
 * @return 0 if device is ready, negative errno if invalid or not ready.
 */
int hal_uart_init(const struct device *uart_dev);

/**
 * @brief Transmit a single byte over UART (polling/blocking).
 *
 * @param uart_dev Pointer to UART device.
 * @param byte     Byte value to send.
 * @return 0 on success, negative errno on failure.
 */
int hal_uart_send_byte(const struct device *uart_dev, uint8_t byte);

/**
 * @brief Transmit a null-terminated string over UART.
 *
 * @param uart_dev Pointer to UART device.
 * @param str      Null-terminated string to send.
 * @return Number of characters sent, or negative errno on failure.
 */
int hal_uart_send_str(const struct device *uart_dev, const char *str);

/**
 * @brief Transmit a binary buffer of given length over UART.
 *
 * @param uart_dev Pointer to UART device.
 * @param data     Buffer containing data to transmit.
 * @param len      Length of data in bytes.
 * @return Number of bytes sent, or negative errno on failure.
 */
int hal_uart_send_buf(const struct device *uart_dev, const uint8_t *data, size_t len);

/**
 * @brief Non-blocking read of a single incoming byte from UART.
 *
 * @param uart_dev Pointer to UART device.
 * @param byte     Pointer to store the received byte.
 * @return 0 if a byte was read, -EAGAIN if no byte available, negative errno on error.
 */
int hal_uart_read_byte(const struct device *uart_dev, uint8_t *byte);

/**
 * @brief Read a single incoming byte from UART with a timeout.
 *
 * @param uart_dev Pointer to UART device.
 * @param byte     Pointer to store received byte.
 * @param timeout  Zephyr timeout (e.g. K_MSEC(100), K_FOREVER, K_NO_WAIT).
 * @return 0 on success, -ETIMEDOUT on timeout, negative errno on error.
 */
int hal_uart_read_byte_timeout(const struct device *uart_dev, uint8_t *byte, k_timeout_t timeout);

/**
 * @brief Read multiple incoming bytes into a buffer up to max_len or timeout.
 *
 * @param uart_dev Pointer to UART device.
 * @param buf      Destination buffer.
 * @param max_len  Maximum number of bytes to read.
 * @param timeout  Timeout for reception.
 * @return Number of bytes actually read.
 */
size_t hal_uart_read_buf(const struct device *uart_dev, uint8_t *buf, size_t max_len, k_timeout_t timeout);

/* ========================================================================= */
/*                           3. TIMER ABSTRACTION                            */
/* ========================================================================= */

/**
 * @brief Callback function type for timer expiration.
 *
 * @param user_data User-provided context pointer.
 */
typedef void (*hal_timer_cb_t)(void *user_data);

/**
 * @brief Modular timer abstraction object encapsulating Zephyr's k_timer.
 */
typedef struct {
    struct k_timer z_timer;
    hal_timer_cb_t cb;
    void *user_data;
    bool is_periodic;
    uint32_t period_ms;
} hal_timer_t;

/**
 * @brief Initialize a hardware/kernel timer abstraction.
 *
 * @param timer     Pointer to hal_timer_t instance.
 * @param cb        Callback to invoke on expiration (ISR context).
 * @param user_data User context pointer passed to callback.
 */
void hal_timer_init(hal_timer_t *timer, hal_timer_cb_t cb, void *user_data);

/**
 * @brief Start a periodic timer.
 *
 * @param timer     Pointer to initialized hal_timer_t instance.
 * @param period_ms Period in milliseconds.
 */
void hal_timer_start_periodic(hal_timer_t *timer, uint32_t period_ms);

/**
 * @brief Start a one-shot timer.
 *
 * @param timer    Pointer to initialized hal_timer_t instance.
 * @param delay_ms Delay in milliseconds before firing.
 */
void hal_timer_start_oneshot(hal_timer_t *timer, uint32_t delay_ms);

/**
 * @brief Stop a running timer.
 *
 * @param timer Pointer to hal_timer_t instance.
 */
void hal_timer_stop(hal_timer_t *timer);

/**
 * @brief Check if timer has fired and fetch its expiration status.
 *
 * @param timer Pointer to hal_timer_t instance.
 * @return Number of expirations since last query.
 */
uint32_t hal_timer_status_get(hal_timer_t *timer);

/**
 * @brief Kernel delay wrapper for milliseconds.
 *
 * @param ms Milliseconds to sleep.
 */
void hal_delay_ms(uint32_t ms);

/**
 * @brief Kernel delay wrapper for microseconds.
 *
 * @param us Microseconds to busy wait/delay.
 */
void hal_delay_us(uint32_t us);

/**
 * @brief Get system uptime in milliseconds since boot.
 *
 * @return Uptime in milliseconds.
 */
uint64_t hal_uptime_ms(void);

/**
 * @brief Get system uptime in microseconds since boot.
 *
 * @return Uptime in microseconds.
 */
uint64_t hal_uptime_us(void);

#ifdef __cplusplus
}
#endif

#endif /* HW_HAL_H_ */
