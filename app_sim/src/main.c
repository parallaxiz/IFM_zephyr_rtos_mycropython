#ifndef NO_QSTR
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>
#endif
#include <string.h>
#include <stdbool.h>
#include "micropython_runner.h"
#include "mod_simulator.h"

static const char main_py_script[] = {
#include "main_py.inc"
    0
};

int main(void)
{
    /* Initialize MicroPython VM environment */
    mp_runner_init();

    /* Run MicroPython runner test suite */
    mp_runner_test_stub();

    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display device not ready!\n");
        mp_runner_deinit();
        return 0;
    }

    display_blanking_off(display_dev);
    printk("SDL Display initialized.\n");

    /* Pass the display device to the Python simulator module */
    simulator_set_display_device(display_dev);

    /* Run application script from main.py */
    mp_run_string(main_py_script);

    mp_runner_deinit();

    return 0;
}