/**
 * @file hw_hal.c
 * @brief Implementation of Lightweight Hardware Abstraction Layer for Zephyr RTOS.
 */

#include "hw_hal.h"
#include <stdarg.h>

#ifndef K_TIMEOUT_EQ
#define K_TIMEOUT_EQ(a, b) ((a).ticks == (b).ticks)
#endif

/* ========================================================================= */
/*                          4. USER-DEFINED LOGGING                          */
/* ========================================================================= */

static hal_log_level_t s_current_log_level = HAL_LOG_LEVEL_INFO;

/* ANSI terminal color codes for clear visual debugging */
#define ANSI_COLOR_RED     "\033[1;31m"
#define ANSI_COLOR_YELLOW  "\033[1;33m"
#define ANSI_COLOR_GREEN   "\033[1;32m"
#define ANSI_COLOR_CYAN    "\033[1;36m"
#define ANSI_COLOR_RESET   "\033[0m"

void hal_log_set_level(hal_log_level_t level)
{
    s_current_log_level = level;
}

hal_log_level_t hal_log_get_level(void)
{
    return s_current_log_level;
}

void hal_log(hal_log_level_t level, const char *tag, const char *fmt, ...)
{
    if (level == HAL_LOG_LEVEL_NONE || level > s_current_log_level) {
        return;
    }

    const char *level_str = "UNK";
    const char *color_code = ANSI_COLOR_RESET;

    switch (level) {
    case HAL_LOG_LEVEL_ERROR:
        level_str = "ERR";
        color_code = ANSI_COLOR_RED;
        break;
    case HAL_LOG_LEVEL_WARN:
        level_str = "WRN";
        color_code = ANSI_COLOR_YELLOW;
        break;
    case HAL_LOG_LEVEL_INFO:
        level_str = "INF";
        color_code = ANSI_COLOR_GREEN;
        break;
    case HAL_LOG_LEVEL_DEBUG:
        level_str = "DBG";
        color_code = ANSI_COLOR_CYAN;
        break;
    default:
        break;
    }

    uint64_t uptime_ms = (uint64_t)k_uptime_get();
    uint32_t sec = (uint32_t)(uptime_ms / 1000ULL);
    uint32_t ms = (uint32_t)(uptime_ms % 1000ULL);

    /* Print timestamp, severity level with color, and tag */
    printk("[%5u.%03u] %s[%s]%s [%s] ",
           sec, ms,
           color_code, level_str, ANSI_COLOR_RESET,
           tag ? tag : "SYS");

    /* Format and print the user message */
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);

    printk("\n");
}

/* ========================================================================= */
/*                            1. GPIO ABSTRACTION                            */
/* ========================================================================= */

int hal_gpio_init(const hal_gpio_pin_t *pin, hal_gpio_dir_t dir, hal_gpio_pull_t pull)
{
    if (pin == NULL || pin->port == NULL) {
        HAL_LOG_ERROR("HAL_GPIO", "Invalid GPIO pin pointer or null controller");
        return -EINVAL;
    }

    if (!device_is_ready(pin->port)) {
        HAL_LOG_ERROR("HAL_GPIO", "GPIO device %s not ready", pin->port->name);
        return -ENODEV;
    }

    gpio_flags_t flags = pin->extra_flags;

    /* Configure direction */
    if (dir == HAL_GPIO_DIR_OUTPUT) {
        flags |= GPIO_OUTPUT;
    } else {
        flags |= GPIO_INPUT;
    }

    /* Configure pull resistors */
    switch (pull) {
    case HAL_GPIO_PULL_UP:
        flags |= GPIO_PULL_UP;
        break;
    case HAL_GPIO_PULL_DOWN:
        flags |= GPIO_PULL_DOWN;
        break;
    case HAL_GPIO_PULL_NONE:
    default:
        break;
    }

    int ret = gpio_pin_configure(pin->port, pin->pin, flags);
    if (ret != 0) {
        HAL_LOG_ERROR("HAL_GPIO", "Failed to configure pin %u on %s (err %d)",
                      pin->pin, pin->port->name, ret);
        return ret;
    }

    HAL_LOG_DEBUG("HAL_GPIO", "Initialized pin %u on %s (dir=%s)",
                  pin->pin, pin->port->name,
                  (dir == HAL_GPIO_DIR_OUTPUT) ? "OUT" : "IN");
    return 0;
}

int hal_gpio_init_output(const hal_gpio_pin_t *pin, int initial_state)
{
    if (pin == NULL || pin->port == NULL) {
        return -EINVAL;
    }

    if (!device_is_ready(pin->port)) {
        HAL_LOG_ERROR("HAL_GPIO", "GPIO device %s not ready", pin->port->name);
        return -ENODEV;
    }

    gpio_flags_t flags = pin->extra_flags;
    flags |= (initial_state ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE);

    int ret = gpio_pin_configure(pin->port, pin->pin, flags);
    if (ret != 0) {
        HAL_LOG_ERROR("HAL_GPIO", "Failed to init output pin %u (err %d)", pin->pin, ret);
        return ret;
    }

    return 0;
}

int hal_gpio_init_input(const hal_gpio_pin_t *pin, hal_gpio_pull_t pull)
{
    return hal_gpio_init(pin, HAL_GPIO_DIR_INPUT, pull);
}

int hal_gpio_set(const hal_gpio_pin_t *pin, bool state)
{
    if (pin == NULL || pin->port == NULL) {
        return -EINVAL;
    }

    return gpio_pin_set(pin->port, pin->pin, state ? 1 : 0);
}

int hal_gpio_toggle(const hal_gpio_pin_t *pin)
{
    if (pin == NULL || pin->port == NULL) {
        return -EINVAL;
    }

    return gpio_pin_toggle(pin->port, pin->pin);
}

int hal_gpio_get(const hal_gpio_pin_t *pin)
{
    if (pin == NULL || pin->port == NULL) {
        return -EINVAL;
    }

    return gpio_pin_get(pin->port, pin->pin);
}

/* ========================================================================= */
/*                            2. UART ABSTRACTION                            */
/* ========================================================================= */

int hal_uart_init(const struct device *uart_dev)
{
    if (uart_dev == NULL) {
        HAL_LOG_ERROR("HAL_UART", "Null UART device pointer");
        return -EINVAL;
    }

    if (!device_is_ready(uart_dev)) {
        HAL_LOG_ERROR("HAL_UART", "UART device %s not ready", uart_dev->name);
        return -ENODEV;
    }

    HAL_LOG_INFO("HAL_UART", "UART device %s verified ready", uart_dev->name);
    return 0;
}

int hal_uart_send_byte(const struct device *uart_dev, uint8_t byte)
{
    if (uart_dev == NULL) {
        return -EINVAL;
    }

    uart_poll_out((struct device *)uart_dev, (unsigned char)byte);
    return 0;
}

int hal_uart_send_str(const struct device *uart_dev, const char *str)
{
    if (uart_dev == NULL || str == NULL) {
        return -EINVAL;
    }

    int count = 0;
    while (*str) {
        uart_poll_out((struct device *)uart_dev, (unsigned char)*str++);
        count++;
    }

    return count;
}

int hal_uart_send_buf(const struct device *uart_dev, const uint8_t *data, size_t len)
{
    if (uart_dev == NULL || data == NULL) {
        return -EINVAL;
    }

    for (size_t i = 0; i < len; i++) {
        uart_poll_out((struct device *)uart_dev, (unsigned char)data[i]);
    }

    return (int)len;
}

int hal_uart_read_byte(const struct device *uart_dev, uint8_t *byte)
{
    if (uart_dev == NULL || byte == NULL) {
        return -EINVAL;
    }

    unsigned char rx_char = 0;
    int ret = uart_poll_in((struct device *)uart_dev, &rx_char);
    if (ret == 0) {
        *byte = (uint8_t)rx_char;
        return 0;
    }

    return -EAGAIN;
}

int hal_uart_read_byte_timeout(const struct device *uart_dev, uint8_t *byte, k_timeout_t timeout)
{
    if (uart_dev == NULL || byte == NULL) {
        return -EINVAL;
    }

    if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
        return hal_uart_read_byte(uart_dev, byte);
    }

    int64_t start_ms = k_uptime_get();
    int64_t timeout_ms = k_ticks_to_ms_near64(timeout.ticks);
    bool forever = K_TIMEOUT_EQ(timeout, K_FOREVER);

    while (1) {
        if (hal_uart_read_byte(uart_dev, byte) == 0) {
            return 0;
        }

        if (!forever && (k_uptime_get() - start_ms >= timeout_ms)) {
            return -ETIMEDOUT;
        }

        /* Yield briefly to avoid hogging CPU while polling */
        k_sleep(K_MSEC(1));
    }
}

size_t hal_uart_read_buf(const struct device *uart_dev, uint8_t *buf, size_t max_len, k_timeout_t timeout)
{
    if (uart_dev == NULL || buf == NULL || max_len == 0) {
        return 0;
    }

    size_t count = 0;
    int64_t start_ms = k_uptime_get();
    int64_t timeout_ms = k_ticks_to_ms_near64(timeout.ticks);
    bool forever = K_TIMEOUT_EQ(timeout, K_FOREVER);

    while (count < max_len) {
        uint8_t byte;
        if (hal_uart_read_byte(uart_dev, &byte) == 0) {
            buf[count++] = byte;
            /* Reset window on new data or continue immediately */
            continue;
        }

        if (!forever && (k_uptime_get() - start_ms >= timeout_ms)) {
            break;
        }

        k_sleep(K_MSEC(1));
    }

    return count;
}

/* ========================================================================= */
/*                           3. TIMER ABSTRACTION                            */
/* ========================================================================= */

static void hal_timer_internal_expiry(struct k_timer *timer_id)
{
    hal_timer_t *timer = (hal_timer_t *)k_timer_user_data_get(timer_id);
    if (timer != NULL && timer->cb != NULL) {
        timer->cb(timer->user_data);
    }
}

void hal_timer_init(hal_timer_t *timer, hal_timer_cb_t cb, void *user_data)
{
    if (timer == NULL) {
        return;
    }

    timer->cb = cb;
    timer->user_data = user_data;
    timer->is_periodic = false;
    timer->period_ms = 0;

    k_timer_init(&timer->z_timer, hal_timer_internal_expiry, NULL);
    k_timer_user_data_set(&timer->z_timer, timer);

    HAL_LOG_DEBUG("HAL_TIMER", "Timer initialized at %p", timer);
}

void hal_timer_start_periodic(hal_timer_t *timer, uint32_t period_ms)
{
    if (timer == NULL) {
        return;
    }

    timer->is_periodic = true;
    timer->period_ms = period_ms;
    k_timer_start(&timer->z_timer, K_MSEC(period_ms), K_MSEC(period_ms));

    HAL_LOG_DEBUG("HAL_TIMER", "Periodic timer started: period=%u ms", period_ms);
}

void hal_timer_start_oneshot(hal_timer_t *timer, uint32_t delay_ms)
{
    if (timer == NULL) {
        return;
    }

    timer->is_periodic = false;
    timer->period_ms = delay_ms;
    k_timer_start(&timer->z_timer, K_MSEC(delay_ms), K_NO_WAIT);

    HAL_LOG_DEBUG("HAL_TIMER", "One-shot timer started: delay=%u ms", delay_ms);
}

void hal_timer_stop(hal_timer_t *timer)
{
    if (timer == NULL) {
        return;
    }

    k_timer_stop(&timer->z_timer);
    HAL_LOG_DEBUG("HAL_TIMER", "Timer stopped at %p", timer);
}

uint32_t hal_timer_status_get(hal_timer_t *timer)
{
    if (timer == NULL) {
        return 0;
    }

    return k_timer_status_get(&timer->z_timer);
}

void hal_delay_ms(uint32_t ms)
{
    k_msleep(ms);
}

void hal_delay_us(uint32_t us)
{
    k_busy_wait(us);
}

uint64_t hal_uptime_ms(void)
{
    return (uint64_t)k_uptime_get();
}

uint64_t hal_uptime_us(void)
{
    return (uint64_t)k_ticks_to_us_near64(k_uptime_ticks());
}
