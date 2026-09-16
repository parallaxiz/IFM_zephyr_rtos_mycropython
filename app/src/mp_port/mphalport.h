#pragma once

#include <stddef.h>
#include "py/mpconfig.h"
#include "../micropython/ports/zephyr/mphalport.h"



mp_uint_t mp_hal_stdout_tx_strn(const char *str, size_t len);