cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_micropython_app)

set(MICROPY_PORT_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/src/micropython/ports/zephyr)
set(MICROPY_DIR         ${MICROPY_PORT_DIR}/../..)
set(MICROPY_OOFATFS_DIR ${MICROPY_DIR}/lib/oofatfs)
set(MICROPY_TARGET      app)

include(${MICROPY_DIR}/py/py.cmake)
include(${MICROPY_DIR}/extmod/extmod.cmake)

set(MICROPY_SOURCE_PORT
    ${MICROPY_PORT_DIR}/help.c
    ${MICROPY_PORT_DIR}/machine_pin.c
    ${MICROPY_PORT_DIR}/machine_i2c.c
    ${MICROPY_PORT_DIR}/machine_spi.c
    ${MICROPY_PORT_DIR}/mphalport.c
    ${MICROPY_PORT_DIR}/uart_core.c
    ${MICROPY_PORT_DIR}/zephyr_device.c
    ${MICROPY_PORT_DIR}/zephyr_storage.c
    ${MICROPY_PORT_DIR}/mpthreadport.c
)

set(MICROPY_SOURCE_SHARED
    ${MICROPY_DIR}/shared/libc/printf.c
    ${MICROPY_DIR}/shared/readline/readline.c
    ${MICROPY_DIR}/shared/runtime/gchelper_generic.c
    ${MICROPY_DIR}/shared/runtime/interrupt_char.c
    ${MICROPY_DIR}/shared/runtime/mpirq.c
    ${MICROPY_DIR}/shared/runtime/pyexec.c
    ${MICROPY_DIR}/shared/runtime/stdout_helpers.c
    ${MICROPY_DIR}/shared/timeutils/timeutils.c
)

set(MICROPY_SOURCE_QSTR
    ${MICROPY_SOURCE_PY}
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_SHARED}
    ${MICROPY_SOURCE_PORT}
)

target_sources(app PRIVATE
    src/main.c
    src/micropython_runner.c
    src/mp_port/mphalport.c
    ${MICROPY_SOURCE_QSTR}
)
