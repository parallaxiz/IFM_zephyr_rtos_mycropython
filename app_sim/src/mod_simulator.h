#ifndef MOD_SIMULATOR_H
#define MOD_SIMULATOR_H

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

void simulator_set_display_device(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* MOD_SIMULATOR_H */
