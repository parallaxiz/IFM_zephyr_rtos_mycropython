#ifndef NO_QSTR
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#endif
#include <string.h>
#include <stdbool.h>
#include "micropython_runner.h"

static const char main_py_script[] = {
#include "main_py.inc"
    0
};

int main(void)
{
    printk("=== [HARDWARE BOOT] Starting Zephyr MicroPython on Target Board ===\n");

    /* Initialize MicroPython VM environment */
    mp_runner_init();

    /* Run self-test suite */
    mp_runner_test_stub();

    printk("=== [HARDWARE] Executing main.py from flash ===\n");

    /* Run application script from main.py */
    mp_run_string(main_py_script);

    mp_runner_deinit();

    printk("=== [HARDWARE] Execution finished ===\n");
    return 0;
}
