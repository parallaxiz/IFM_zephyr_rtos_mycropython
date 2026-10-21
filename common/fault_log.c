/**
 * @file fault_log.c
 * @brief Persistent Fault Logger and Crash Loop Detector using Zephyr NVS.
 */

#include "fault_log.h"
#include "hw_hal.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#if defined(CONFIG_NVS) && defined(CONFIG_FLASH_MAP)
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#define NVS_AVAILABLE 1
#else
#define NVS_AVAILABLE 0
#endif

/* NVS Key IDs */
#define NVS_KEY_BOOT_COUNT    1
#define NVS_KEY_CRASH_STREAK  2
#define NVS_KEY_FAULT_HEAD    3
#define NVS_KEY_FAULT_BASE    10

static bool s_initialized = false;
static bool s_nvs_ready = false;

/* RAM Fallback structures in case Flash partition is unavailable */
static uint32_t s_ram_boot_count = 0;
static uint32_t s_ram_crash_streak = 0;
static uint32_t s_ram_fault_head = 0;
static fault_record_t s_ram_fault_records[FAULT_LOG_MAX_ENTRIES];

#if NVS_AVAILABLE
static struct nvs_fs s_fs;
#endif

int fault_log_init(void)
{
    if (s_initialized) {
        return 0;
    }

#if NVS_AVAILABLE
#if FIXED_PARTITION_EXISTS(storage_partition)
    const struct flash_area *fa;
    int rc = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
    if (rc == 0) {
        struct flash_pages_info info;
        rc = flash_get_page_info_by_offs(flash_area_get_device(fa), fa->fa_off, &info);
        if (rc == 0) {
            s_fs.flash_device = flash_area_get_device(fa);
            s_fs.offset = fa->fa_off;
            s_fs.sector_size = (uint16_t)info.size;
            s_fs.sector_count = (uint16_t)(fa->fa_size / info.size);

            rc = nvs_mount(&s_fs);
            if (rc == 0) {
                s_nvs_ready = true;
                HAL_LOG_INFO("FAULT_LOG", "NVS Flash filesystem mounted successfully on storage_partition (%u bytes)",
                             (unsigned int)fa->fa_size);
            } else {
                HAL_LOG_WARN("FAULT_LOG", "nvs_mount failed (%d), falling back to RAM log", rc);
            }
        } else {
            HAL_LOG_WARN("FAULT_LOG", "flash_get_page_info_by_offs failed (%d)", rc);
        }
        flash_area_close(fa);
    } else {
        HAL_LOG_WARN("FAULT_LOG", "storage_partition not accessible (%d), using RAM storage", rc);
    }
#else
    HAL_LOG_INFO("FAULT_LOG", "storage_partition not defined in devicetree; operating in memory-backed mode");
#endif
#endif

    s_initialized = true;
    return 0;
}

int fault_log_boot_counter_increment(uint32_t *out_boot_count, bool *out_crash_loop)
{
    fault_log_init();

    uint32_t boot_cnt = 0;
    uint32_t crash_streak = 0;

    if (s_nvs_ready) {
#if NVS_AVAILABLE
        int bytes = nvs_read(&s_fs, NVS_KEY_BOOT_COUNT, &boot_cnt, sizeof(boot_cnt));
        if (bytes <= 0) {
            boot_cnt = 0;
        }

        bytes = nvs_read(&s_fs, NVS_KEY_CRASH_STREAK, &crash_streak, sizeof(crash_streak));
        if (bytes <= 0) {
            crash_streak = 0;
        }

        boot_cnt++;
        crash_streak++;

        (void)nvs_write(&s_fs, NVS_KEY_BOOT_COUNT, &boot_cnt, sizeof(boot_cnt));
        (void)nvs_write(&s_fs, NVS_KEY_CRASH_STREAK, &crash_streak, sizeof(crash_streak));
#endif
    } else {
        s_ram_boot_count++;
        s_ram_crash_streak++;
        boot_cnt = s_ram_boot_count;
        crash_streak = s_ram_crash_streak;
    }

    if (out_boot_count != NULL) {
        *out_boot_count = boot_cnt;
    }

    bool loop_detected = (crash_streak >= FAULT_LOG_CRASH_LOOP_THRESHOLD);
    if (out_crash_loop != NULL) {
        *out_crash_loop = loop_detected;
    }

    HAL_LOG_INFO("FAULT_LOG", "Boot Counter: %u | Consecutive Crash Streak: %u", boot_cnt, crash_streak);

    if (loop_detected) {
        HAL_LOG_ERROR("FAULT_LOG", "CRASH LOOP DETECTED! Streak=%u exceeds threshold (%u)",
                      crash_streak, FAULT_LOG_CRASH_LOOP_THRESHOLD);
        fault_log_record(FAULT_TYPE_CRASH_LOOP_DETECTED, (uint16_t)crash_streak,
                         "Repeated system reboots without stable runtime checkpoint");
    }

    return 0;
}

int fault_log_get_boot_count(uint32_t *out_boot_count)
{
    if (out_boot_count == NULL) {
        return -EINVAL;
    }

    fault_log_init();

    if (s_nvs_ready) {
#if NVS_AVAILABLE
        uint32_t boot_cnt = 0;
        int bytes = nvs_read(&s_fs, NVS_KEY_BOOT_COUNT, &boot_cnt, sizeof(boot_cnt));
        if (bytes > 0) {
            *out_boot_count = boot_cnt;
            return 0;
        }
#endif
    }

    *out_boot_count = s_ram_boot_count;
    return 0;
}

int fault_log_clear_crash_streak(void)
{
    fault_log_init();

#if NVS_AVAILABLE
    if (s_nvs_ready) {
        uint32_t zero = 0;
        int rc = nvs_write(&s_fs, NVS_KEY_CRASH_STREAK, &zero, sizeof(zero));
        if (rc < 0) {
            return rc;
        }
    }
#endif
    s_ram_crash_streak = 0;

    HAL_LOG_INFO("FAULT_LOG", "System stable: crash streak reset to 0");
    return 0;
}

int fault_log_record(fault_type_t type, uint16_t code, const char *msg)
{
    fault_log_init();

    fault_record_t record;
    memset(&record, 0, sizeof(record));
    record.timestamp_ms = (uint32_t)hal_uptime_ms();
    record.fault_type = (uint16_t)type;
    record.code = code;

    if (msg != NULL) {
        strncpy(record.message, msg, sizeof(record.message) - 1);
        record.message[sizeof(record.message) - 1] = '\0';
    }

    if (s_nvs_ready) {
#if NVS_AVAILABLE
        uint32_t head = 0;
        int bytes = nvs_read(&s_fs, NVS_KEY_FAULT_HEAD, &head, sizeof(head));
        if (bytes <= 0) {
            head = 0;
        }

        uint16_t key = (uint16_t)(NVS_KEY_FAULT_BASE + (head % FAULT_LOG_MAX_ENTRIES));
        int rc = nvs_write(&s_fs, key, &record, sizeof(record));
        if (rc >= 0) {
            head++;
            (void)nvs_write(&s_fs, NVS_KEY_FAULT_HEAD, &head, sizeof(head));
            HAL_LOG_WARN("FAULT_LOG", "Recorded fault [type=%u, code=%u] to NVS key %u: %s",
                         record.fault_type, record.code, key, record.message);
            return 0;
        }
#endif
    }

    /* Fallback to RAM buffer */
    uint32_t slot = s_ram_fault_head % FAULT_LOG_MAX_ENTRIES;
    s_ram_fault_records[slot] = record;
    s_ram_fault_head++;

    HAL_LOG_WARN("FAULT_LOG", "Recorded fault [type=%u, code=%u] to RAM slot %u: %s",
                 record.fault_type, record.code, slot, record.message);
    return 0;
}

int fault_log_dump(void)
{
    fault_log_init();

    HAL_LOG_INFO("FAULT_LOG", "========== PERSISTENT FAULT LOG DUMP ==========");

    int count = 0;
    if (s_nvs_ready) {
#if NVS_AVAILABLE
        uint32_t head = 0;
        int bytes = nvs_read(&s_fs, NVS_KEY_FAULT_HEAD, &head, sizeof(head));
        if (bytes <= 0) {
            head = 0;
        }

        uint32_t start = (head < FAULT_LOG_MAX_ENTRIES) ? 0 : (head - FAULT_LOG_MAX_ENTRIES);

        for (uint32_t i = start; i < head; i++) {
            uint16_t key = (uint16_t)(NVS_KEY_FAULT_BASE + (i % FAULT_LOG_MAX_ENTRIES));
            fault_record_t entry;
            bytes = nvs_read(&s_fs, key, &entry, sizeof(entry));
            if (bytes == sizeof(entry)) {
                HAL_LOG_INFO("FAULT_LOG", "[#%u | Uptime %5u ms] Type: %u, Code: %u, Msg: \"%s\"",
                             count + 1, entry.timestamp_ms, entry.fault_type, entry.code, entry.message);
                count++;
            }
        }
#endif
    } else {
        uint32_t start = (s_ram_fault_head < FAULT_LOG_MAX_ENTRIES) ? 0 : (s_ram_fault_head - FAULT_LOG_MAX_ENTRIES);

        for (uint32_t i = start; i < s_ram_fault_head; i++) {
            uint32_t slot = i % FAULT_LOG_MAX_ENTRIES;
            fault_record_t *e = &s_ram_fault_records[slot];
            HAL_LOG_INFO("FAULT_LOG", "[#%u | RAM Uptime %5u ms] Type: %u, Code: %u, Msg: \"%s\"",
                         count + 1, e->timestamp_ms, e->fault_type, e->code, e->message);
            count++;
        }
    }

    if (count == 0) {
        HAL_LOG_INFO("FAULT_LOG", "No recorded faults found (clean history).");
    }

    HAL_LOG_INFO("FAULT_LOG", "================================================");
    return count;
}

int fault_log_clear_all(void)
{
    fault_log_init();

    if (s_nvs_ready) {
#if NVS_AVAILABLE
        uint32_t zero = 0;
        (void)nvs_write(&s_fs, NVS_KEY_BOOT_COUNT, &zero, sizeof(zero));
        (void)nvs_write(&s_fs, NVS_KEY_CRASH_STREAK, &zero, sizeof(zero));
        (void)nvs_write(&s_fs, NVS_KEY_FAULT_HEAD, &zero, sizeof(zero));
        for (int i = 0; i < FAULT_LOG_MAX_ENTRIES; i++) {
            (void)nvs_delete(&s_fs, NVS_KEY_FAULT_BASE + i);
        }
#endif
    }

    s_ram_boot_count = 0;
    s_ram_crash_streak = 0;
    s_ram_fault_head = 0;
    memset(s_ram_fault_records, 0, sizeof(s_ram_fault_records));

    HAL_LOG_INFO("FAULT_LOG", "Persistent fault log and boot counter cleared.");
    return 0;
}
