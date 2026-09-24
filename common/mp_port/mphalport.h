#pragma once

#include <stddef.h>
#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "../micropython/ports/zephyr/mphalport.h"

int mp_hal_stdin_rx_chr(void);
mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len);