#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <stddef.h>
#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "mphalport.h"

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
    uintptr_t ret = 0;
    if (poll_flags & MP_STREAM_POLL_RD) {
        #ifdef CONFIG_CONSOLE_SUBSYS
        // Console subsystem doesn't expose a non-blocking readability check,
        // so unconditionally report readable (matches upstream Zephyr port).
        ret |= MP_STREAM_POLL_RD;
        #endif
    }
    if (poll_flags & MP_STREAM_POLL_WR) {
        ret |= MP_STREAM_POLL_WR;
    }
    return ret;
}

int mp_hal_stdin_rx_chr(void) {
    return console_getchar();
}

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printk("%c", str[i]);
    }
    return len;
}

