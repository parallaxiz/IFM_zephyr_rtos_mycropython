#pragma once

#include <stddef.h>
#include "py/mpconfig.h"

#ifndef NO_QSTR
#include <zephyr/sys/printk.h>
#endif

static inline void mp_hal_set_interrupt_char(char c) {
    (void)c;
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len);