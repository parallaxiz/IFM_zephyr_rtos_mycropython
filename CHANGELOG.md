# Changelog

All notable changes to the **Zephyr RTOS + MicroPython Firmware Platform** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.1.0] - 2026-10-22

### Added
- **Persistent Flight Recorder & Fault Log:** Non-volatile flash-backed logging (`common/fault_log.c/.h`) surviving MCU resets and power loss using Zephyr NVS.
- **Hardware Boot Counter:** Persistent reboot tracking to identify unexpected restarts and crash loops.
- **Crash Loop Safeguard:** System automatically detects repeated rapid crashes ($\ge 3$) and initiates safety pauses before executing new scripts.
- **Thread Safety VM Mutex:** Implemented `pm_vm_lock()` and `pm_vm_unlock()` in `common/power_mgmt.c/.h` to protect MicroPython runtime against concurrent thread re-entry.
- **GPIO Low-Power Deconfiguration:** Added `pm_gpio_set_low_power()` using `GPIO_DISCONNECTED` to stop quiescent leakage current across floating pins.
- **Event-Driven UART Interrupt Reception:** Replaced legacy busy-polling loops with interrupt-driven callbacks (`pm_uart_enable_rx_event_driven()`) enabling tickless idle sleep.
- **Power Profiling Report:** Published empirical energy characterization report (`docs/power_profiling.md`) capturing current draw across 6 operating modes on nRF5340 DK using Nordic PPK2.
- **Hardware Test Suite:** Added 7 automated MicroPython validation scripts (`test_scripts/`) covering GPIO, input buttons, timer interrupts, UART, math, exceptions, and GC allocation.
- **Firmware Versioning Subsystem:** Added `common/version.h` defining semantic versioning macros and build timestamps.
- **Defects Ledger:** Created `DEFECTS.md` cataloging known system defects, root cause analyses, and permanent fixes.
- **Boilerplate Compatibility Application:** Fixed broken `app/src/` path by providing a standalone entrypoint matching `README.md` instructions.

### Changed
- Expanded `app_sim/testcase.yaml` with rigorous regex patterns testing MicroPython boot, arithmetic, exception recovery, and HAL integrity.
- Optimized GitHub Actions CI/CD workflow (`.github/workflows/ci.yml`) to build both native simulation and target apps with cached dependencies.

---

## [1.0.0] - 2026-09-24

### Added
- Initial project scaffolding for Zephyr RTOS (v4.4+) running on `native_sim` and `nRF5340 DK`.
- MicroPython core integration with custom Zephyr port (`common/mp_port/`).
- Lightweight Hardware Abstraction Layer (`hw_hal.c/.h`) covering GPIO, UART, Timer, and colored severity logging.
- UART script streaming runner supporting dynamic execution of Python scripts ending with `Ctrl+D` (`0x04`).
- Native simulator SDL display integration for PC execution testing.
