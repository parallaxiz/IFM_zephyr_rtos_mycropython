#include "mod_simulator.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>

#include "py/runtime.h"
#include "py/obj.h"

#define RECT_SIZE       64
#define COLOR_GREEN     0xFF00E000u
#define COLOR_DARK_GREY 0xFF3C3C3Cu

static const struct device *sim_display_dev = NULL;
static uint32_t buf[RECT_SIZE * RECT_SIZE];

void simulator_set_display_device(const struct device *dev) {
    sim_display_dev = dev;
}

static void fill_rect_internal(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    if (!sim_display_dev) {
        return;
    }
    for (int i = 0; i < w * h && i < RECT_SIZE * RECT_SIZE; i++) {
        buf[i] = color;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = w * h * sizeof(uint32_t),
        .width    = w,
        .height   = h,
        .pitch    = w,
    };

    display_write(sim_display_dev, x, y, &desc, buf);
}

static mp_obj_t mod_simulator_set_led(mp_obj_t state_in) {
    bool state = mp_obj_is_true(state_in);
    if (state) {
        printk("[SIMULATOR] LED: ON (Bright Green)\n");
        fill_rect_internal(128, 88, RECT_SIZE, RECT_SIZE, COLOR_GREEN);
    } else {
        printk("[SIMULATOR] LED: OFF (Dark Grey)\n");
        fill_rect_internal(128, 88, RECT_SIZE, RECT_SIZE, COLOR_DARK_GREY);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mod_simulator_set_led_obj, mod_simulator_set_led);

static mp_obj_t mod_simulator_sleep_ms(mp_obj_t ms_in) {
    mp_int_t ms = mp_obj_get_int(ms_in);
    if (ms > 0) {
        k_msleep(ms);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mod_simulator_sleep_ms_obj, mod_simulator_sleep_ms);

static const mp_rom_map_elem_t simulator_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_simulator) },
    { MP_ROM_QSTR(MP_QSTR_set_led), MP_ROM_PTR(&mod_simulator_set_led_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep_ms), MP_ROM_PTR(&mod_simulator_sleep_ms_obj) },
};
static MP_DEFINE_CONST_DICT(simulator_module_globals, simulator_module_globals_table);

const mp_obj_module_t mp_module_simulator = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&simulator_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_simulator, mp_module_simulator);
