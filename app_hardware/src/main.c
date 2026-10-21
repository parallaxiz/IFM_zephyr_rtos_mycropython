#ifndef NO_QSTR
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/console/console.h>
#endif
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/lexer.h"
#include "py/parse.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "micropython_runner.h"
#include "hw_hal.h"

/* Simple self-test demonstrating all 4 HAL abstractions on physical target */
static volatile uint32_t s_timer_ticks = 0;
static void test_timer_callback(void *user_data)
{
    hal_gpio_pin_t *led = (hal_gpio_pin_t *)user_data;
    if (led != NULL) {
        hal_gpio_toggle(led);
    }
    s_timer_ticks++;
}

static void run_hal_self_test(void)
{
    HAL_LOG_INFO("TEST", "========================================");
    HAL_LOG_INFO("TEST", "Starting Hardware HAL Self-Test on nRF Board");
    HAL_LOG_INFO("TEST", "========================================");

    /* 1. Logging verification */
    HAL_LOG_DEBUG("TEST", "Testing DEBUG level message");
    HAL_LOG_INFO("TEST",  "Testing INFO level message with uptime: %llu ms", hal_uptime_ms());
    HAL_LOG_WARN("TEST",  "Testing WARN level message");
    HAL_LOG_ERROR("TEST", "Testing ERROR level message");

    /* 2. UART Abstraction */
    const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (hal_uart_init(uart) == 0) {
        hal_uart_send_str(uart, "[HAL_UART] Direct UART string transmission verified!\r\n");
    }

    /* 3. GPIO Abstraction (using board's led0 alias) */
#if DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
    const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
    hal_gpio_pin_t led_pin = {
        .port = led_spec.port,
        .pin = led_spec.pin,
        .extra_flags = led_spec.dt_flags,
    };
    HAL_LOG_INFO("TEST", "Configuring LED0 on port %s pin %u", led_pin.port->name, led_pin.pin);
#else
    hal_gpio_pin_t led_pin = HAL_GPIO_PIN_INIT(DEVICE_DT_GET(DT_NODELABEL(gpio0)), 28);
#endif

    if (hal_gpio_init_output(&led_pin, 1) == 0) {
        HAL_LOG_INFO("TEST", "LED initial state: %d", hal_gpio_get(&led_pin));
        hal_delay_ms(250);
        hal_gpio_set(&led_pin, false);
        HAL_LOG_INFO("TEST", "LED state after write(0): %d", hal_gpio_get(&led_pin));
    }

    /* 4. Timer Abstraction (blink LED for 1.5 seconds) */
    hal_timer_t timer;
    hal_timer_init(&timer, test_timer_callback, &led_pin);
    HAL_LOG_INFO("TEST", "Starting 200ms periodic timer (blinking LED)...");
    hal_timer_start_periodic(&timer, 200);

    /* Wait 1.5 seconds to observe timer ticks & LED blinking */
    hal_delay_ms(1500);

    hal_timer_stop(&timer);
    HAL_LOG_INFO("TEST", "Timer stopped. Total callback firings: %u", s_timer_ticks);
    HAL_LOG_INFO("TEST", "HAL Self-Test Complete! Proceeding to main application...");
    HAL_LOG_INFO("TEST", "========================================");
}

#define SCRIPT_BUFFER_SIZE 8192
#define CHAR_CTRL_C 0x03
#define CHAR_CTRL_D 0x04

static char script_buffer[SCRIPT_BUFFER_SIZE];

/**
 * @brief Parse, compile, and execute a Python script string in the MicroPython VM.
 *
 * @param src Null-terminated string containing Python source code.
 */
static void do_str(const char *src)
{
    if (src == NULL || strlen(src) == 0) {
        return;
    }

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
        if (lex == NULL) {
            printk("[HARDWARE] Error: Failed to create MicroPython lexer.\n");
            nlr_pop();
            return;
        }

        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);
        mp_call_function_0(module_fun);
        nlr_pop();
    } else {
        /* Exception caught inside nlr_push block */
        printk("[HARDWARE] MicroPython Exception Caught:\n");
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
    }
}

int main(void)
{
    printk("=== [HARDWARE BOOT] Starting Zephyr MicroPython UART Script Streamer ===\n");

    /* Run Hardware Abstraction Layer self-test */
    run_hal_self_test();

    /* Initial boot of MicroPython runtime */
    mp_runner_init();
    mp_runner_deinit();

    /* Initialize Zephyr console subsystem for character input */
    if (console_init() != 0) {
        printk("[HARDWARE] Error: console_init() failed!\n");
        return -1;
    }

    printk("[HARDWARE] Console initialized successfully.\n");

    while (1) {
        printk("\n=== [HARDWARE] Awaiting Python script stream over UART (End with Ctrl+D / 0x04) ===\n");

        size_t script_len = 0;

        while (1) {
            int c = console_getchar();
            if (c < 0) {
                k_msleep(10);
                continue;
            }

            if (c == CHAR_CTRL_D) {
                /* End of transmission */
                break;
            } else if (c == CHAR_CTRL_C) {
                /* Cancel/Reset input buffer */
                printk("\n[HARDWARE] Stream input canceled via Ctrl+C.\n");
                script_len = 0;
                break;
            } else {
                if (script_len < sizeof(script_buffer) - 1) {
                    script_buffer[script_len++] = (char)c;
                } else {
                    printk("\n[HARDWARE] Warning: Script buffer full, ignoring incoming characters!\n");
                }
            }
        }

        if (script_len == 0) {
            continue;
        }

        script_buffer[script_len] = '\0';

        printk("=== [HARDWARE] Received script (%u bytes). Initializing MicroPython VM... ===\n", (unsigned int)script_len);

        /* Initialize MicroPython VM environment for fresh execution */
        mp_runner_init();

        printk("=== [HARDWARE] Dynamically executing script ===\n");
        do_str(script_buffer);

        /* Clean up MicroPython VM state */
        mp_runner_deinit();
        printk("=== [HARDWARE] Script execution completed, VM deinitialized ===\n");
    }

    return 0;
}

