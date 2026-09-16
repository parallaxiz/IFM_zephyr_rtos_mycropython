#include <zephyr/kernel.h>
#include <stddef.h>
#include "py/mpconfig.h"
#include "mphalport.h"

mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printk("%c", str[i]);
    }
    return len;
}
