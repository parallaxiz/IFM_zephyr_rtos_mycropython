# Defects Ledger & Resolution Log

**Subsystem:** Embedded Zephyr RTOS + MicroPython Platform  
**Owner:** 👤 Person 3 — Validation, CI/CD & Release  
**Status:** Active  

---

## Defects Registry

### DEF-001: Missing Application Boilerplate (`app/src/`)
* **Severity:** High (Setup Blocker)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** Running the documented setup command `west build -b native_sim app` failed immediately with:
  ```
  FATAL ERROR: source directory "app" does not exist
  ```
* **Root Cause:** Application directory was split into `app_hardware` and `app_sim`, but the standard entrypoint `app/` referenced by `README.md` was missing from the repository.
* **Resolution:** Re-created `app/CMakeLists.txt`, `app/prj.conf`, `app/src/main.c`, and `app/testcase.yaml` to ensure out-of-the-box build compliance for all new contributors.

---

### DEF-002: Uninitialized MicroPython Submodule on Fresh Clone
* **Severity:** High (Build Blocker)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** Fresh `git clone` produced compilation failure: `ffconf.h: No such file or directory` or missing core MicroPython files.
* **Root Cause:** Git submodules are not recursively checked out during standard `git clone` without `--recurse-submodules`.
* **Resolution:** Configured `submodules: recursive` in `.github/workflows/ci.yml` and documented `git submodule update --init --recursive` in setup scripts.

---

### DEF-003: Quiescent GPIO Leakage Current After Script Execution
* **Severity:** Medium (Battery / Power Penalty)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** On the physical nRF5340 target, baseline idle current remained at **215 µA** after MicroPython finished executing a script.
* **Root Cause:** Inactive pins were left driven high/low or floating in high-impedance input mode, leading to continuous microamp leakage through internal pull networks and input buffer transistors.
* **Resolution:** Person 2 implemented `pm_gpio_set_low_power()` and `pm_gpio_restore_all_low_power()` in `common/power_mgmt.c`. Inactive pins are now explicitly reconfigured with `GPIO_DISCONNECTED`, dropping quiescent current to **28.4 µA** (86.8% reduction).

---

### DEF-004: Unsafe Multi-Threaded Re-entry into MicroPython VM
* **Severity:** Critical (Memory Corruption / HardFault)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** If a background thread (e.g. UART RX or timer) invoked Python execution while another script was running, heap state corrupted, triggering an `Uncaught NLR exception` panic.
* **Root Cause:** MicroPython's runtime engine utilizes single global state structures (`MP_STATE_VM`, `MP_STATE_THREAD`) and is not re-entrant without mutex synchronization.
* **Resolution:** Created a thread-safety mutex abstraction (`pm_vm_lock()` / `pm_vm_unlock()`) in `common/power_mgmt.c` utilizing Zephyr's `k_mutex`.

---

### DEF-005: Undetected Hardware Crash / Reboot Loops
* **Severity:** High (Field Reliability)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** A fatal exception during script startup triggered repeated MCU restarts without persistent diagnostics, leaving target boards in continuous reboot loops.
* **Root Cause:** Absence of non-volatile storage logging across system resets.
* **Resolution:** Created `common/fault_log.c/.h` using Zephyr Flash NVS (`storage_partition`). Implemented a persistent boot counter and crash streak detector that flags reboots exceeding threshold ($\ge 3$) and initiates safety pauses.

---

### DEF-006: Deprecated SPI Master Macro in Zephyr v4.4+
* **Severity:** Low (Compiler Warning)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** Compilation warning:
  ```
  warning: 'SPI_OP_MODE_MASTER' is deprecated: Use SPI_OP_MODE_CONTROLLER instead
  ```
* **Root Cause:** Zephyr RTOS updated terminology from master/slave to controller/peripheral across SPI subsystem drivers.
* **Resolution:** Added `-Wno-error=deprecated-declarations` in `CMakeLists.txt` and updated HAL headers.

---

### DEF-007: Native Simulator Runner Discarded Section on Newer Host GCC
* **Severity:** Medium (Simulation Host Linker Issue)
* **Status:** ✅ RESOLVED
* **Reported Behavior:** On modern Linux distros (GCC 13/14), linking the native simulator runner executable failed with:
  ```
  `__x86.get_pc_thunk.bx' referenced in section `.text' of zephyr.elf.loc_cpusw.o: defined in discarded section
  ```
* **Root Cause:** Newer GCC versions enforce position-independent executable (`-pie`) defaults, conflicting with 32-bit `-m32` non-pie object merging in the Zephyr native runner Makefile.
* **Resolution:** Added `-no-pie` / `-fno-pie` to host compiler flags and documented the workaround in `README.md`.
