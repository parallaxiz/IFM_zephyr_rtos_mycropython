#include "micropython_runner.h"

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

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
#include "shared/runtime/gchelper.h"

static char mp_heap[16384];
static bool mp_is_initialized = false;

void mp_runner_init(void) {
    if (!mp_is_initialized) {
        volatile int stack_dummy = 0;
        mp_stack_set_top((void *)&stack_dummy);
        mp_stack_set_limit(4096 - 512);
        gc_init(mp_heap, mp_heap + sizeof(mp_heap));
        mp_init();
        mp_is_initialized = true;
    }
}

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    gc_collect_end();
}

NORETURN void nlr_jump_fail(void *val) {
    printk("[MP_RUNNER] FATAL: Uncaught NLR exception!\n");
    while (1) {
        k_sleep(K_FOREVER);
    }
}

int mp_run_string(const char *src) {
    if (src == NULL) {
        printk("[MP_RUNNER] Error: NULL script pointer provided.\n");
        return -1;
    }

    mp_runner_init();

    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        // Step 1: Initialize lexer from string source
        mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, src, strlen(src), 0);
        if (lex == NULL) {
            printk("[MP_RUNNER] Error: Failed to create MicroPython lexer.\n");
            nlr_pop();
            return -1;
        }

        // Step 2: Parse source code into abstract syntax tree
        qstr source_name = lex->source_name;
        mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);

        // Step 3: Compile parse tree to bytecode module function
        mp_obj_t module_fun = mp_compile(&parse_tree, source_name, false);

        // Step 4: Execute top-level module code
        mp_call_function_0(module_fun);

        nlr_pop();
        printk("[MP_RUNNER] Script executed successfully.\n");
        return 0;
    } else {
        // Exception caught inside nlr_push block
        printk("[MP_RUNNER] MicroPython Exception Caught:\n");
        mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
        return -1;
    }
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
