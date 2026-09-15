#pragma once

#include <stdint.h>
#include <alloca.h>

#define MICROPY_ENABLE_GC             (1)
#define MICROPY_HELPER_RELOADS        (0)
#define MICROPY_LONGINT_IMPL          (MICROPY_LONGINT_IMPL_NONE)
#define MICROPY_ENABLE_SOURCE_LINE    (1)
#define MICROPY_FLOAT_IMPL            (MICROPY_FLOAT_IMPL_NONE)
#define MICROPY_ERROR_REPORTING       (MICROPY_ERROR_REPORTING_TERSE)
#define MICROPY_PY_BUILTINS_BYTEARRAY (0)
#define MICROPY_PY_ARRAY              (0)
#define MICROPY_KBD_EXCEPTION         (0)

#define MICROPY_PY_SYS                (0)
#define MICROPY_ENABLE_COMPILER       (0)

#define MICROPY_NLR_SETJMP            (1)
#define MICROPY_OBJ_REPR              (MICROPY_OBJ_REPR_A)

typedef intptr_t mp_int_t;
typedef uintptr_t mp_uint_t;
typedef intptr_t mp_off_t;

#define MP_SSIZE_MAX                  INTPTR_MAX

#define MICROPY_PORT_BUILTINS
#define MICROPY_PORT_ROOT_POINTERS