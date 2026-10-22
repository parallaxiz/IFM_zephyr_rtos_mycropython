#include "micropython_runner.h"
#include "mp_safety.h"
#include "power_mgmt.h"
#include "fault_log.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>

/* MicroPython core headers */
#include "py/mpconfig.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
#include "py/lexer.h"
#include "py/parse.h"
#include "py/nlr.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "shared/runtime/gchelper.h"

#ifndef MICROPY_HEAP_SIZE
#define MICROPY_HEAP_SIZE (48 * 1024)
#endif

/* Maximum allowed Python script size -- reject oversized scripts before execution */
#define MP_SCRIPT_MAX_SIZE 6000U

static char mp_heap[MICROPY_HEAP_SIZE];
static bool mp_is_initialized = false;

static struct k_timer s_execution_timer;
static bool s_timer_initialized = false;

static void execution_timer_expiry_cb(struct k_timer *timer_id)
{
    ARG_UNUSED(timer_id);
    printk("[MP_RUNNER] Soft execution timeout (5s) expired! Safely aborting VM...\n");
#if MICROPY_ENABLE_VM_ABORT
    mp_sched_vm_abort();
#endif
    mp_safety_wdt_kick();
}

void mp_runner_init(void) {
    if (!mp_is_initialized) {
        if (!s_timer_initialized) {
            k_timer_init(&s_execution_timer, execution_timer_expiry_cb, NULL);
            s_timer_initialized = true;
        }

        volatile int stack_dummy = 0;
        mp_stack_ctrl_init();
        mp_stack_set_top((void *)&stack_dummy);
        mp_stack_set_limit(4096 - 512);

        gc_init(mp_heap, mp_heap + sizeof(mp_heap));
        printk("[VM_INIT] Allocated %d KB GC Heap [%p - %p]\n",
               (int)(sizeof(mp_heap) / 1024),
               (void *)mp_heap, (void *)(mp_heap + sizeof(mp_heap)));

        mp_init();
        mp_hal_init();
        mp_is_initialized = true;
        printk("MicroPython initialized successfully\n");
    }
}

void mp_runner_deinit(void) {
    if (mp_is_initialized) {
        k_timer_stop(&s_execution_timer);
        mp_deinit();
        mp_is_initialized = false;
        printk("[VM_INIT] MicroPython runtime deinitialized.\n");
    }
}

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}

/*
 * nlr_jump_fail() is defined in mp_safety.c to trigger sys_reboot()
 * instead of spinning forever.
 */

int mp_run_string(const char *src) {
    if (src == NULL) {
        printk("[MP_RUNNER] Error: NULL script pointer provided.\n");
        return -1;
    }

    /* Safety gate 1: enforce script size limit */
    if (!mp_safety_script_allowed(src)) {
        printk("[MP_RUNNER] Script rejected by safety layer.\n");
        return -1;
    }

    size_t src_len = strlen(src);

    /* Thread safety: acquire VM execution mutex */
    pm_vm_lock(K_FOREVER);

    mp_runner_init();

    int result = 0;
    nlr_buf_t nlr;
#if MICROPY_ENABLE_VM_ABORT
    nlr_set_abort(&nlr);
#endif
    if (nlr_push(&nlr) == 0) {
        // Step 1: Initialize lexer from string source
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, src_len, 0);
        if (lex == NULL) {
            printk("[MP_RUNNER] Error: Failed to create MicroPython lexer.\n");
            nlr_pop();
#if MICROPY_ENABLE_VM_ABORT
            nlr_set_abort(NULL);
#endif
            pm_vm_unlock();
            return -1;
        }

        // Step 2: Parse source code into abstract syntax tree
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);

        // Step 3: Compile parse tree to bytecode module function
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);

        // Step 4: Start 5-second one-shot soft execution timeout
        k_timer_start(&s_execution_timer, K_SECONDS(5), K_NO_WAIT);

        // Execute top-level module code
        mp_call_function_0(module_fun);

        // Execution completed cleanly: immediately stop timer
        k_timer_stop(&s_execution_timer);

        nlr_pop();
#if MICROPY_ENABLE_VM_ABORT
        nlr_set_abort(NULL);
#endif
        printk("[MP_RUNNER] Script executed successfully.\n");

        /* Safety gate 2: check heap utilisation after execution */
        mp_safety_heap_check();

        /* Record stable operation to clear crash streaks */
        fault_log_clear_crash_streak();

        /* Low-power: restore GPIO lines to disconnected state */
        pm_gpio_restore_all_low_power();

        result = 0;
    } else {
        // Exception or timeout abort caught inside nlr_push block
        k_timer_stop(&s_execution_timer);
#if MICROPY_ENABLE_VM_ABORT
        nlr_set_abort(NULL);
#endif
        mp_safety_wdt_kick();

        if (nlr.ret_val == NULL) {
            printk("[MP_RUNNER] Trapped soft execution timeout (5s limit exceeded). Board stays alive.\n");
            fault_log_record(FAULT_TYPE_PYTHON_EXCEPTION, 2, "Script timeout (5s limit)");
        } else {
            printk("[MP_RUNNER] MicroPython Exception Caught:\n");
            mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
            fault_log_record(FAULT_TYPE_PYTHON_EXCEPTION, 1, "Script exception");
        }

        result = -1;
    }

    pm_vm_unlock();
    return result;
}

void mp_runner_test_stub(void) {
    printk("\n--- [MP_RUNNER TEST STUB] Starting Tests ---\n");

    // Test 1: Standard valid Python string
    const char *test_script_valid =
        "print('Test: MicroPython string runner working!')\n"
        "a = 10\n"
        "b = 20\n"
        "print('10 + 20 =', a + b)\n";

    printk("[TEST 1] Running valid script...\n");
    int res1 = mp_run_string(test_script_valid);
    printk("[TEST 1] Result: %d\n", res1);

    // Test 2: Intentional Exception (ZeroDivisionError)
    const char *test_script_exception =
        "print('Attempting division by zero...')\n"
        "x = 1 / 0\n";

    printk("\n[TEST 2] Running script with intentional exception...\n");
    int res2 = mp_run_string(test_script_exception);
    printk("[TEST 2] Result: %d (Expected failure caught safely)\n", res2);

    printk("--- [MP_RUNNER TEST STUB] Tests Completed ---\n\n");
}
