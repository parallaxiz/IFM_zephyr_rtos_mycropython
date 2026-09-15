cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_micropython_app)

set(MP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/micropython)

target_include_directories(app PRIVATE
    src/mp_port
    ${MP_DIR}
    ${MP_DIR}/py
)

# Compile core VM runtime, GC heap, and required objects for Task 2.2
target_sources(app PRIVATE
    src/main.c
    ${MP_DIR}/py/mpstate.c
    ${MP_DIR}/py/malloc.c
    ${MP_DIR}/py/gc.c
    ${MP_DIR}/py/pystack.c
    ${MP_DIR}/py/qstr.c
    ${MP_DIR}/py/vstr.c
    ${MP_DIR}/py/mpprint.c
    ${MP_DIR}/py/unicode.c
    ${MP_DIR}/py/runtime.c
    ${MP_DIR}/py/runtime_utils.c
    ${MP_DIR}/py/obj.c
    ${MP_DIR}/py/objtype.c
    ${MP_DIR}/py/objsingleton.c
    ${MP_DIR}/py/objbool.c
    ${MP_DIR}/py/objint.c
    ${MP_DIR}/py/objstr.c
    ${MP_DIR}/py/objtuple.c
    ${MP_DIR}/py/objlist.c
    ${MP_DIR}/py/objfun.c
    ${MP_DIR}/py/objdict.c
    ${MP_DIR}/py/objnone.c
    ${MP_DIR}/py/objexcept.c
    ${MP_DIR}/py/smallint.c
    ${MP_DIR}/py/stackctrl.c
    ${MP_DIR}/py/argcheck.c
    ${MP_DIR}/py/warning.c
    ${MP_DIR}/py/map.c
    ${MP_DIR}/py/sequence.c
    ${MP_DIR}/py/objobject.c
    ${MP_DIR}/py/objproperty.c
    ${MP_DIR}/py/objpolyiter.c
    ${MP_DIR}/py/objslice.c
    ${MP_DIR}/py/opmethods.c
    ${MP_DIR}/py/binary.c
    ${MP_DIR}/py/parsenum.c
    ${MP_DIR}/py/parsenumbase.c
    ${MP_DIR}/py/nlrsetjmp.c
    ${MP_DIR}/py/nlr.c
    ${MP_DIR}/py/objboundmeth.c
    ${MP_DIR}/py/objgetitemiter.c
    ${MP_DIR}/py/stream.c
    ${MP_DIR}/py/modbuiltins.c
)