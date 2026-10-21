/**
 * @file power_mgmt.c
 * @brief Implementation of Low-Power Management, GPIO Leakage Prevention & VM Mutex.
 */

#include "power_mgmt.h"
#include "hw_hal.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* ========================================================================= */
/*                   1. THREAD SAFETY MUTEX FOR VM EXECUTION                 */
/* ========================================================================= */

static K_MUTEX_DEFINE(s_pm_vm_mutex);
static volatile bool s_vm_locked = false;

int pm_vm_lock(k_timeout_t timeout)
{
    int ret = k_mutex_lock(&s_pm_vm_mutex, timeout);
    if (ret == 0) {
        s_vm_locked = true;
    }
    return ret;
}

void pm_vm_unlock(void)
{
    s_vm_locked = false;
    k_mutex_unlock(&s_pm_vm_mutex);
}

bool pm_vm_is_locked(void)
{
    return s_vm_locked;
}

/* ========================================================================= */
/*               2. GPIO LOW-POWER STATE (LEAKAGE PREVENTION)                */
/* ========================================================================= */

int pm_gpio_set_low_power(const struct device *port, gpio_pin_t pin)
{
    if (port == NULL) {
        return -EINVAL;
    }

    if (!device_is_ready(port)) {
        HAL_LOG_ERROR("POWER_MGMT", "GPIO port %s not ready", port->name);
        return -ENODEV;
    }

    /* Configure pin as GPIO_DISCONNECTED to disable both input buffer and output driver */
    int ret = gpio_pin_configure(port, pin, GPIO_DISCONNECTED);
    if (ret != 0) {
        HAL_LOG_WARN("POWER_MGMT", "Failed to disconnect pin %u on %s (err %d)", pin, port->name, ret);
        return ret;
    }

    HAL_LOG_DEBUG("POWER_MGMT", "Pin %u on %s set to low-power disconnected state", pin, port->name);
    return 0;
}

int pm_gpio_set_hal_pin_low_power(const hal_gpio_pin_t *hal_pin)
{
    if (hal_pin == NULL || hal_pin->port == NULL) {
        return -EINVAL;
    }

    return pm_gpio_set_low_power(hal_pin->port, hal_pin->pin);
}

int pm_gpio_restore_all_low_power(void)
{
    HAL_LOG_INFO("POWER_MGMT", "Disconnecting active peripherals to eliminate quiescent leakage...");

    int disconnected_count = 0;

#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    if (device_is_ready(led0.port)) {
        (void)gpio_pin_configure(led0.port, led0.pin, GPIO_DISCONNECTED);
        disconnected_count++;
    }
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(led1), okay)
    const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
    if (device_is_ready(led1.port)) {
        (void)gpio_pin_configure(led1.port, led1.pin, GPIO_DISCONNECTED);
        disconnected_count++;
    }
#endif

    HAL_LOG_INFO("POWER_MGMT", "Successfully put %d peripheral pin(s) into low-power disconnected state",
                 disconnected_count);
    return 0;
}

/* ========================================================================= */
/*              3. EVENT-DRIVEN UART RECEPTION (TICKLESS IDLE)               */
/* ========================================================================= */

static pm_uart_rx_cb_t s_rx_cb = NULL;
static void *s_rx_user_data = NULL;

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
static void pm_uart_isr(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    uart_irq_update((struct device *)dev);
    while (uart_irq_is_pending((struct device *)dev)) {
        if (uart_irq_rx_ready((struct device *)dev)) {
            uint8_t byte = 0;
            while (uart_fifo_read((struct device *)dev, &byte, 1) > 0) {
                if (s_rx_cb != NULL) {
                    s_rx_cb(byte, s_rx_user_data);
                }
            }
        }
        uart_irq_update((struct device *)dev);
    }
}
#endif

int pm_uart_enable_rx_event_driven(const struct device *uart_dev, pm_uart_rx_cb_t cb, void *user_data)
{
    if (uart_dev == NULL || cb == NULL) {
        return -EINVAL;
    }

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
    s_rx_cb = cb;
    s_rx_user_data = user_data;

    uart_irq_callback_user_data_set((struct device *)uart_dev, pm_uart_isr, NULL);
    uart_irq_rx_enable((struct device *)uart_dev);

    HAL_LOG_INFO("POWER_MGMT", "Event-driven UART RX interrupts enabled on %s (tickless idle active)",
                 uart_dev->name);
    return 0;
#else
    HAL_LOG_WARN("POWER_MGMT", "CONFIG_UART_INTERRUPT_DRIVEN not enabled; event-driven UART unsupported");
    return -ENOTSUP;
#endif
}

int pm_uart_disable_rx_event_driven(const struct device *uart_dev)
{
    if (uart_dev == NULL) {
        return -EINVAL;
    }

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
    uart_irq_rx_disable((struct device *)uart_dev);
    s_rx_cb = NULL;
    s_rx_user_data = NULL;
    HAL_LOG_DEBUG("POWER_MGMT", "Event-driven UART RX interrupts disabled on %s", uart_dev->name);
    return 0;
#else
    return 0;
#endif
}

void pm_enter_low_power_idle(k_timeout_t duration)
{
    k_sleep(duration);
}

void pm_self_test(void)
{
    HAL_LOG_INFO("PM_TEST", "========================================");
    HAL_LOG_INFO("PM_TEST", "Running Person 2 Power & Concurrency Test");
    HAL_LOG_INFO("PM_TEST", "========================================");

    /* 1. Test VM Mutex Lock / Unlock */
    int rc = pm_vm_lock(K_MSEC(100));
    HAL_LOG_INFO("PM_TEST", "Acquired VM mutex: rc=%d (locked=%s)", rc, pm_vm_is_locked() ? "true" : "false");
    pm_vm_unlock();
    HAL_LOG_INFO("PM_TEST", "Released VM mutex: (locked=%s)", pm_vm_is_locked() ? "true" : "false");

    /* 2. Test GPIO Disconnection Leakage Prevention */
    pm_gpio_restore_all_low_power();

    HAL_LOG_INFO("PM_TEST", "Person 2 Self-Test Complete!");
    HAL_LOG_INFO("PM_TEST", "========================================");
}
