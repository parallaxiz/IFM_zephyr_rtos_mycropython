#pragma once

#ifndef NO_QSTR
#include <zephyr/sys/printk.h>
#endif

static inline void mp_hal_set_interrupt_char(char c) {
    (void)c;
}