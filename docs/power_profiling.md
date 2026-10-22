# Power Profiling & Low-Power Optimization Architecture Report

**Subsystem:** Embedded Zephyr RTOS + MicroPython Runtime Engine  
**Hardware Target:** Nordic Semiconductor nRF5340 DK (PCA10095 - Application Core Cortex-M33)  
**Author:** Person 2 (Low Power, Thread Safety & Fault Log Lead)  
**Status:** Architecture Implemented / Physical Metrics Pending Bench PPK2 Measurement  

---

## 1. Executive Summary

This engineering report documents the low-power architecture, power state transitions, and expected current consumption profiles for the embedded Zephyr RTOS + MicroPython platform on the nRF5340 DK.

> [!IMPORTANT]
> **Factual Accuracy Notice:** All low-power architectural mechanisms described in this document (event-driven UART ISRs, pin disconnection via `GPIO_DISCONNECTED`, Zephyr tickless idle via `CONFIG_PM=y`, and VM concurrency mutexes) are fully implemented and verified in firmware. However, exact milliampere/microampere figures require a physical Nordic Power Profiler Kit II (PPK2) test jig connected to header P22. In simulator environments (`native_sim`), CPU cycles are host-driven and physical current cannot be measured. All bench metrics below are explicitly marked as **Pending physical measurement via Power Profiler Kit II (PPK2)**.

The primary architectural optimizations implemented in code:
1. **Elimination of Busy-Polling:** Replaced polling UART loops (`console_getchar()` / `k_msleep(10)`) with event-driven hardware interrupt reception (`pm_uart_enable_rx_event_driven()`) and Zephyr tickless idle (`CONFIG_PM=y`).
2. **GPIO Quiescent Leakage Prevention:** Implemented automatic pin deconfiguration to disconnected high-impedance state (`pm_gpio_restore_all_low_power()` using `GPIO_DISCONNECTED`) immediately after MicroPython script execution finishes.
3. **Thread-Safe Runtime Execution:** Enforced mutex locks around VM script execution (`pm_vm_lock()` / `pm_vm_unlock()`) to prevent multithreaded race conditions without introducing busy-wait current penalties.
4. **Crash-Resilient NVS Storage:** Integrated Flash Non-Volatile Storage (NVS) for boot counting and persistent fault logging with localized sector writes.

---

## 2. Measurement Equipment & Test Setup Specification

When running physical laboratory verification on hardware, the following configuration must be used:

### 2.1 Hardware Configuration
- **Current Measurement Tool:** Nordic Semiconductor Power Profiler Kit II (PPK2).
- **Operating Mode:** Ampere Meter Mode (sampling rate: 100 kHz / 100,000 samples/sec).
- **Supply Voltage (VDD):** 3.0V DC regulated from PPK2 source.
- **Target Board:** nRF5340 DK (PCA10095).
  - Current measurement header: **P22** (cut solder bridge **SB40** to route current through PPK2).
  - VDD nRF power switch: **SW8** set to **ON**.
  - Power switch: **SW9** set to **VDD**.
- **Isolation:** Segger J-Link debugger and interface MCU USB disconnected from PC during standalone power measurements to eliminate ground-loop leakage and back-powering over SWD / VIO pins.

```
       +-------------------------------------------------------+
       |             Nordic Power Profiler Kit II             |
       |  [ VOUT (3.0V) ] ------------------> [ P22 Pin 1 ]   |
       |  [ GND         ] ------------------> [ P22 Pin 2 / GND|
       +-------------------------------------------------------+
                                  |
                                  v
                      +-----------------------+
                      |   nRF5340 DK Target   |
                      | (App Core Cortex-M33) |
                      |    P22 Current Port   |
                      +-----------------------+
```

---

## 3. Power State Matrix & Measurement Status

The firmware defines 6 distinct operational power phases:

| Operating State | Description / Firmware Mechanism | Theoretical Target (Nordic nRF5340 PS v1.5 @ 3.0V) | Bench Measurement Status |
| :--- | :--- | :--- | :--- |
| **State 1: Active Execution** | Cortex-M33 @ 128 MHz running from Flash/Cache; compiling & executing MicroPython bytecode. | ~7.0 – 9.5 mA | *Pending physical measurement via Power Profiler Kit II (PPK2)* |
| **State 2: Active UART RX** | Receiving framed script payload over UART at 115200 baud with UARTE EasyDMA active. | ~5.0 – 6.5 mA | *Pending physical measurement via Power Profiler Kit II (PPK2)* |
| **State 3: Awaiting Script (Legacy Polling)** | Legacy loop calling `console_getchar()` with `k_msleep(10)`. Periodic timer wakeups. | ~4.0 – 4.5 mA | *Eliminated in firmware architecture* |
| **State 4: Awaiting Script (Event-Driven)** | UART RX interrupts enabled; CPU in WFI (Wait-For-Interrupt) tickless idle (`CONFIG_PM=y`). | ~1.2 – 1.8 mA | *Pending physical measurement via Power Profiler Kit II (PPK2)* |
| **State 5: Post-Execution Quiescent Idle** | MicroPython VM deinitialized; all GPIOs reset via `pm_gpio_restore_all_low_power()` (`GPIO_DISCONNECTED`). | < 35 µA | *Pending physical measurement via Power Profiler Kit II (PPK2)* |
| **State 6: Deep Sleep / Standby** | System in low-power System OFF standby with RAM retention enabled; external pin wakeup. | ~1.5 – 3.0 µA | *Pending physical measurement via Power Profiler Kit II (PPK2)* |

---

## 4. Technical Analysis of Low-Power Firmware Mechanisms

### 4.1 GPIO Leakage Current Elimination
- **Problem:** When a MicroPython script finishes controlling external hardware (e.g., toggling LEDs, driving sensor chips, or configuring input buttons), pins left configured as outputs or floating inputs continuously leak current through internal push-pull MOSFET networks or pull resistors. A single pin left floating near switching threshold can leak 50 µA to 120 µA.
- **Solution:** Implemented `pm_gpio_restore_all_low_power()` in `common/power_mgmt.c`. The function reconfigures inactive peripheral pins with the Zephyr flag `GPIO_DISCONNECTED`. This completely disables both input buffer and output driver stages (`PIN_CNF[n].CONNECT = 1`).
- **Bench Verification:** *Pending physical measurement via Power Profiler Kit II (PPK2).*

### 4.2 Event-Driven UART Reception vs. Polling
- **Problem:** Legacy streaming read loop in `main.c` repeatedly executed `k_msleep(10)` or spun in busy loops polling `console_getchar()`. This forced the RTOS kernel tick timer to interrupt the CPU 100 times per second, preventing low-power sleep modes.
- **Solution:** Configured `CONFIG_UART_INTERRUPT_DRIVEN=y` and implemented `pm_uart_enable_rx_event_driven()`. The CPU remains in low-power WFI sleep state until the UART hardware FIFO triggers an interrupt upon receiving a byte.
- **Bench Verification:** *Pending physical measurement via Power Profiler Kit II (PPK2).*

### 4.3 Thread Safety Mutex Overhead
- **Implementation:** Added a dedicated Zephyr kernel mutex `s_vm_mutex` accessed via `pm_vm_lock()` and `pm_vm_unlock()`.
- **Bench Verification:** Mutex uses deterministic kernel primitives (`k_mutex_lock` / `k_mutex_unlock`) with no active CPU spinning or polling current penalty.

### 4.4 Non-Volatile Flash Storage (NVS) Energy Cost
- **Implementation:** Boot counter and fault records are saved using Zephyr NVS in flash partition `storage_partition`.
- **Bench Verification:** NVS writes execute in brief burst cycles during exception/boot events, leaving quiescent battery draw unchanged during normal execution. *Pending physical measurement via Power Profiler Kit II (PPK2).*

---

## 5. Step-by-Step Guide to Execute Physical Bench Measurements

Once the physical hardware test jig is set up:
1. **Connect PPK2 to nRF5340 DK:**
   - Connect PPK2 **VOUT** to pin 1 of header **P22**.
   - Connect PPK2 **GND** to DK GND header.
   - Verify solder bridge **SB40** is cut.
2. **Launch nRF Connect Power Profiler:**
   - Open **nRF Connect for Desktop** -> **Power Profiler**.
   - Select PPK2 COM port and choose **Source Meter** mode.
   - Set voltage to **3000 mV** (3.0V) and click **Enable Power**.
3. **Capture & Record Verified State Metrics:**
   - **Boot & Self-Test:** Record current curve during LED toggle self-test.
   - **Post-Self-Test Idle:** Record current once `pm_gpio_restore_all_low_power()` executes.
   - **Script Reception & Execution:** Stream a Python script via `tools/send_script.py`; record active execution current and the transition back to low-power quiescent idle.
   - Update Section 3 with verified laboratory measurements and scope capture traces.
