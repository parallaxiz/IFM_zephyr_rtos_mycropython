/**
 * @file version.h
 * @brief Firmware Versioning Subsystem for Zephyr RTOS + MicroPython Platform.
 *
 * Authored by Person 3 (Validation, CI/CD & Release Lead).
 */

#ifndef FIRMWARE_VERSION_H_
#define FIRMWARE_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VERSION_MAJOR 1
#define FW_VERSION_MINOR 1
#define FW_VERSION_PATCH 0

#define FW_VERSION_TAG   "release"

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif
#ifndef TOSTRING
#define TOSTRING(x)  STRINGIFY(x)
#endif

#define FW_VERSION_STRING \
    "v" TOSTRING(FW_VERSION_MAJOR) "." TOSTRING(FW_VERSION_MINOR) "." TOSTRING(FW_VERSION_PATCH)

#define FW_BUILD_TIMESTAMP __DATE__ " " __TIME__

/**
 * @brief Retrieve human-readable firmware version string.
 *
 * @return Formatted version string (e.g. "v1.1.0 (Oct 22 2026 00:45:00)").
 */
static inline const char *fw_version_get_string(void)
{
    return FW_VERSION_STRING;
}

/**
 * @brief Retrieve firmware build date and time.
 *
 * @return Build timestamp string.
 */
static inline const char *fw_version_get_build_timestamp(void)
{
    return FW_BUILD_TIMESTAMP;
}

#ifdef __cplusplus
}
#endif

#endif /* FIRMWARE_VERSION_H_ */
