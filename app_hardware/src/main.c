/**
 * @file main.c  (app_hardware)
 * @brief Hardware application entry point.
 *
 * Receives Python scripts over UART1 using the CRC-16 framing protocol
 * defined in uart_protocol.h, then executes them in the MicroPython VM.
 *
 * Safety features:
 *  - Watchdog: system auto-reboots if the main loop stalls
 *  - Heap guard: checked after every script execution
 *  - Script size limit: enforced before passing to the VM
 *  - Module whitelist: only approved modules may be imported
 *  - nlr_jump_fail(): triggers cold reboot instead of spinning
 *
 * Hardware target: nRF5340 DK (nrf5340dk/nrf5340/cpuapp)
 *   Console:        UART0 (on-board USB CDC-ACM) -> printk / debug output
 *   Script UART:    UART1 (P1.02=TX, P1.01=RX, 115200 baud, no h/w flow)
 */

#ifndef NO_QSTR
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#endif

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* MicroPython headers (needed for the inline do_str helper) */
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/lexer.h"
#include "py/parse.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/mphal.h"

/* Project headers */
#include "micropython_runner.h"
#include "hw_hal.h"
#include <zephyr/drivers/watchdog.h>
#include "uart_protocol.h"
#include "mp_safety.h"

/* ========================================================================= */
/*                     Script UART device binding                             */
/* ========================================================================= */

/*
 * Use DT alias "script-uart" defined in the .overlay file.
 * Falls back gracefully when building for native_sim (alias absent).
 */
#if DT_NODE_EXISTS(DT_ALIAS(script_uart))
#  define SCRIPT_UART_NODE DT_ALIAS(script_uart)
#else
/* native_sim fallback – use the console UART so CI still builds */
#  define SCRIPT_UART_NODE DT_CHOSEN(zephyr_console)
#endif

static const struct device *s_script_uart;

/* Static receive buffer (sized to match protocol limit + NUL byte) */
static char s_script_buf[UART_PROTO_MAX_SCRIPT_LEN + 1u];

/* ========================================================================= */
/*                          HAL self-test                                     */
/* ========================================================================= */

static volatile uint32_t s_timer_ticks = 0u;

static void test_timer_callback(void *user_data)
{
    hal_gpio_pin_t *pin = (hal_gpio_pin_t *)user_data;
    s_timer_ticks++;
    hal_gpio_toggle(pin);
}

/**
 * @brief Quick HAL smoke test: UART tx, GPIO output, periodic timer.
 *
 * Runs once at startup to confirm HAL primitives work before the
 * script receiver loop is entered.
 */
static void run_hal_self_test(void)
{
    HAL_LOG_INFO("TEST", "========================================");
    HAL_LOG_INFO("TEST", "HAL Self-Test Starting...");

    /* 1. UART abstraction – send a banner on the console UART */
    const struct device *console_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (hal_uart_init(console_uart) == 0) {
        hal_uart_send_str(console_uart,
                          "[HAL_UART] Direct UART string transmission verified!\r\n");
    }

    /* 2. GPIO Abstraction (using board's led0 alias when available) */
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    hal_gpio_pin_t led_pin = {
        .port        = led_spec.port,
        .pin         = led_spec.pin,
        .extra_flags = led_spec.dt_flags,
    };
    HAL_LOG_INFO("TEST", "Configuring LED0 on port %s pin %u",
                 led_pin.port->name, led_pin.pin);
#else
    hal_gpio_pin_t led_pin = HAL_GPIO_PIN_INIT(DEVICE_DT_GET(DT_NODELABEL(gpio0)), 28);
#endif

    if (hal_gpio_init_output(&led_pin, 1) == 0) {
        HAL_LOG_INFO("TEST", "LED initial state: %d", hal_gpio_get(&led_pin));
        hal_delay_ms(250);
        hal_gpio_set(&led_pin, false);
        HAL_LOG_INFO("TEST", "LED state after write(0): %d", hal_gpio_get(&led_pin));
    }

    /* 3. Timer abstraction – blink LED for 1.5 s using a 200 ms periodic timer */
    hal_timer_t timer;
    hal_timer_init(&timer, test_timer_callback, &led_pin);
    HAL_LOG_INFO("TEST", "Starting 200 ms periodic timer (blinking LED)...");
    hal_timer_start_periodic(&timer, 200u);
    hal_delay_ms(1500u);
    hal_timer_stop(&timer);

    HAL_LOG_INFO("TEST", "Timer stopped. Total callback firings: %u", s_timer_ticks);
    HAL_LOG_INFO("TEST", "HAL Self-Test Complete! Proceeding to main application...");
    HAL_LOG_INFO("TEST", "========================================");
}

/* ========================================================================= */
/*                          Main entry point                                  */
/* ========================================================================= */

int main(void)
{
    printk("=== [HARDWARE BOOT] Zephyr MicroPython UART Script Streamer ===\n");
    printk("[BOOT] Protocol: CRC-16 framing (uart_protocol.c)\n");
    printk("[BOOT] Safety:   watchdog + heap guard + size limit + whitelist\n");

    /* ------------------------------------------------------------------ */
    /* 1. Initialise the watchdog – must be first to avoid silent hangs   */
    /* ------------------------------------------------------------------ */
    int wdt_ret = mp_safety_wdt_init();
    if (wdt_ret != 0 && wdt_ret != -ENODEV) {
        /* Non-fatal: log and continue without WDT */
        printk("[BOOT] WARNING: WDT init returned %d\n", wdt_ret);
    }
    mp_safety_wdt_kick();

    /* ------------------------------------------------------------------ */
    /* 2. HAL smoke test                                                   */
    /* ------------------------------------------------------------------ */
    run_hal_self_test();
    mp_safety_wdt_kick();

    /* ------------------------------------------------------------------ */
    /* 3. Verify script UART device                                        */
    /* ------------------------------------------------------------------ */
    s_script_uart = DEVICE_DT_GET(SCRIPT_UART_NODE);
    if (!device_is_ready(s_script_uart)) {
        printk("[BOOT] ERROR: Script UART device '%s' not ready!\n",
               s_script_uart->name);
        return -EIO;
    }
    printk("[BOOT] Script UART '%s' ready at 115200 baud.\n",
           s_script_uart->name);

    /* ------------------------------------------------------------------ */
    /* 4. Boot MicroPython once to pre-initialise internals               */
    /* ------------------------------------------------------------------ */
    mp_runner_init();
    mp_runner_deinit();
    mp_safety_wdt_kick();

    /* ------------------------------------------------------------------ */
    /* 5. Main receive-execute loop                                        */
    /* ------------------------------------------------------------------ */
    printk("\n[MAIN] Entering script receive loop.\n");
    printk("[MAIN] Send a framed Python script (magic=0xABCD, CRC-16).\n\n");

    while (1) {
        /* Kick WDT at the top of each iteration */
        mp_safety_wdt_kick();

        printk("[MAIN] Waiting for framed script on '%s'...\n",
               s_script_uart->name);

        size_t script_len = 0u;
        int    recv_ret   = uart_proto_receive_frame(s_script_uart,
                                                    s_script_buf,
                                                    sizeof(s_script_buf),
                                                    &script_len);

        mp_safety_wdt_kick();  /* frame could take a while to arrive */

        if (recv_ret != UART_PROTO_OK) {
            switch (recv_ret) {
            case UART_PROTO_ERR_CRC:
                printk("[MAIN] Frame discarded: CRC mismatch.\n");
                /* Send NACK */
                uart_proto_send_frame(s_script_uart, "NACK:CRC");
                break;
            case UART_PROTO_ERR_TOO_LARGE:
                printk("[MAIN] Frame discarded: payload too large.\n");
                uart_proto_send_frame(s_script_uart, "NACK:SIZE");
                break;
            case UART_PROTO_ERR_TIMEOUT:
                printk("[MAIN] Frame discarded: inter-byte timeout.\n");
                uart_proto_send_frame(s_script_uart, "NACK:TIMEOUT");
                break;
            default:
                printk("[MAIN] Frame error: %d\n", recv_ret);
                uart_proto_send_frame(s_script_uart, "NACK:ERR");
                break;
            }
            continue;
        }

        /* Empty ping frame – just ACK and loop */
        if (script_len == 0u) {
            printk("[MAIN] Ping frame received, sending ACK.\n");
            uart_proto_send_frame(s_script_uart, "ACK:PING");
            continue;
        }

        printk("[MAIN] Received %zu-byte script. Validating...\n", script_len);

        /* Safety: size is already checked by uart_protocol, but double-check */
        if (!mp_safety_script_allowed(s_script_buf)) {
            printk("[MAIN] Script rejected by safety layer.\n");
            uart_proto_send_frame(s_script_uart, "NACK:SAFETY");
            continue;
        }

        /* ACK receipt before VM execution (let host know we have it) */
        uart_proto_send_frame(s_script_uart, "ACK:RX");

        /* Init VM, run script, deinit */
        printk("[MAIN] Initialising MicroPython VM...\n");
        mp_runner_init();
        mp_safety_wdt_kick();

        printk("[MAIN] Executing script...\n");
        int exec_ret = mp_run_string(s_script_buf);
        mp_safety_wdt_kick();

        mp_runner_deinit();

        if (exec_ret == 0) {
            printk("[MAIN] Script OK.\n");
            uart_proto_send_frame(s_script_uart, "ACK:EXEC_OK");
        } else {
            printk("[MAIN] Script execution error.\n");
            uart_proto_send_frame(s_script_uart, "NACK:EXEC_ERR");
        }

        /* Heap guard – may reboot if heap is critically low */
        mp_safety_heap_check();
    }

    /* Unreachable */
    return 0;
}
