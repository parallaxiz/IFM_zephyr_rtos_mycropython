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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/micropython_runner.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mp_port/mphalport.c
    ${MICROPY_SOURCE_PY}
    ${MICROPY_SOURCE_EXTMOD}
    ${MICROPY_SOURCE_SHARED}
    ${MICROPY_SOURCE_PORT}
)

target_sources(app PRIVATE
    ${MICROPY_SOURCE_QSTR}
)

target_include_directories(app PRIVATE
    src
    src/mp_port
    ${MICROPY_PORT_DIR}
    ${MICROPY_DIR}
    ${MICROPY_INC_CORE}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/genhdr
    ${CMAKE_CURRENT_BINARY_DIR}/zephyr/include/generated
    ${CMAKE_CURRENT_BINARY_DIR}/zephyr/include/generated/zephyr
)

target_compile_definitions(app PRIVATE
    NDEBUG
    MICROPY_HEAP_SIZE=16384
    FFCONF_H="${MICROPY_OOFATFS_DIR}/ffconf.h"
    asm=__asm__
)

zephyr_get_include_directories_for_lang(C includes)
zephyr_get_system_include_directories_for_lang(C system_includes)
zephyr_get_compile_definitions_for_lang(C definitions)
zephyr_get_compile_options_for_lang(C options)

set(MICROPY_CPP_FLAGS_EXTRA -I${CMAKE_CURRENT_SOURCE_DIR}/src/mp_port -I${CMAKE_CURRENT_SOURCE_DIR}/src -I${MICROPY_PORT_DIR} -I${MICROPY_DIR} -I${MICROPY_DIR}/py ${includes} ${system_includes} ${definitions} ${options} -I${CMAKE_CURRENT_BINARY_DIR}/zephyr/include/generated/zephyr -I${CMAKE_CURRENT_BINARY_DIR}/zephyr/include/generated -DFFCONF_H="${MICROPY_OOFATFS_DIR}/ffconf.h" -Dasm=__asm__)

include(${MICROPY_DIR}/py/mkrules.cmake)
