#ifndef NO_QSTR
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>
#endif
#include <string.h>

/* MicroPython core headers */
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define RECT_SIZE     64

#define COLOR_GREEN     0xFF00E000u
#define COLOR_DARK_GREY 0xFF3C3C3Cu

/* Task 2.2: 48 KB Dedicated Heap for MicroPython Garbage Collection */
#define MICROPY_HEAP_SIZE (48 * 1024)
static char mp_heap[MICROPY_HEAP_SIZE];

static uint32_t buf[RECT_SIZE * RECT_SIZE];

void fill_rect(const struct device *display_dev,
               uint16_t x, uint16_t y,
               uint16_t w, uint16_t h,
               uint32_t color)
{
    for (int i = 0; i < w * h; i++) {
        buf[i] = color;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = w * h * sizeof(uint32_t),
        .width    = w,
        .height   = h,
        .pitch    = w,
    };

    display_write(display_dev, x, y, &desc, buf);
}

/* MicroPython runtime hooks */
void nlr_jump_fail(void *val)
{
    printk("[MICROPY_ERR] Fatal NLR jump failed: %p\n", val);
    while (1) {
        k_sleep(K_FOREVER);
    }
}

void gc_collect(void)
{
    gc_collect_start();
    gc_collect_end();
}

/* Task 2.2: VM Runtime Setup */
void mp_runtime_setup(void)
{
    printk("[VM_INIT] Initializing MicroPython runtime...\n");

    mp_stack_ctrl_init();
    mp_stack_set_limit(4096 - 512);

    gc_init(mp_heap, mp_heap + sizeof(mp_heap));
    printk("[VM_INIT] Allocated 48 KB GC Heap [%p - %p]\n",
           (void *)mp_heap, (void *)(mp_heap + sizeof(mp_heap)));

    mp_init();
    printk("[VM_INIT] MicroPython VM runtime initialized successfully.\n");
}

/* Task 2.2: VM Runtime Teardown */
void mp_runtime_cleanup(void)
{
    mp_deinit();
    printk("[VM_INIT] MicroPython runtime deinitialized.\n");
}

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display device not ready!\n");
        return 0;
    }

    display_blanking_off(display_dev);
    printk("SDL Display initialized.\n");

    mp_runtime_setup();

    bool led_state = false;
    for (int i = 0; i < 4; i++) {
        led_state = !led_state;

        if (led_state) {
            printk("[SIMULATOR] LED: ON (Bright Green)\n");
            fill_rect(display_dev, 128, 88, RECT_SIZE, RECT_SIZE, COLOR_GREEN);
        } else {
            printk("[SIMULATOR] LED: OFF (Dark Grey)\n");
            fill_rect(display_dev, 128, 88, RECT_SIZE, RECT_SIZE, COLOR_DARK_GREY);
        }

        k_msleep(1000);
    }

    mp_runtime_cleanup();

    return 0;
}