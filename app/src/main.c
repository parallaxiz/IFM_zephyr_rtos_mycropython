#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/display.h>
#include <string.h>

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define RECT_SIZE     64

/* ARGB8888 colors: 0xAARRGGBB */
#define COLOR_GREEN     0xFF00E000u   /* bright green  */
#define COLOR_DARK_GREY 0xFF3C3C3Cu  /* dark grey     */

/* Static buffer: RECT_SIZE x RECT_SIZE pixels @ 4 bytes each (ARGB8888) */
static uint32_t buf[RECT_SIZE * RECT_SIZE];

void fill_rect(const struct device *display_dev,
               uint16_t x, uint16_t y,
               uint16_t w, uint16_t h,
               uint32_t color)
{
    for (int i = 0; i < w * h; i++) {
        buf[i] = color;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = w * h * sizeof(uint32_t), /* 4 bytes per pixel (ARGB8888) */
        .width    = w,
        .height   = h,
        .pitch    = w,
    };

    display_write(display_dev, x, y, &desc, buf);
}

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        printk("Display device not ready!\n");
        return 0;
    }

    /* Exit blanking mode — display shows nothing until this is called */
    display_blanking_off(display_dev);

    printk("SDL Display initialized. Starting simulated LED flashing...\n");

    bool led_state = false;

    while (1) {
        led_state = !led_state;

        if (led_state) {
            printk("[SIMULATOR] LED: ON (Bright Green)\n");
            fill_rect(display_dev, 128, 88, RECT_SIZE, RECT_SIZE, COLOR_GREEN);
        } else {
            printk("[SIMULATOR] LED: OFF (Dark Grey)\n");
            fill_rect(display_dev, 128, 88, RECT_SIZE, RECT_SIZE, COLOR_DARK_GREY);
        }

        k_msleep(1000); /* Toggle every 1 second */
    }

    return 0;
}
