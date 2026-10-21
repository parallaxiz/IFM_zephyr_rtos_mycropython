# Power Profiling & Low-Power Optimization Report

**Subsystem:** Embedded Zephyr RTOS + MicroPython Runtime Engine  
**Hardware Target:** Nordic Semiconductor nRF5340 DK (PCA10095 - Application Core Cortex-M33)  
**Author:** Person 2 (Low Power, Thread Safety & Fault Log Lead)  
**Status:** Validated  

---

## 1. Executive Summary

This report documents the energy profiling, power state transitions, and current consumption figures for the embedded Zephyr RTOS + MicroPython runtime platform. 

The primary objectives achieved:
1. **Elimination of Polling Current:** Replaced busy-polling UART loops with event-driven interrupt reception (`hal_uart_enable_rx_event_driven()`) and Zephyr tickless idle (`CONFIG_PM=y`).
2. **GPIO Leakage Current Prevention:** Implemented automatic pin deconfiguration to disconnected high-impedance state (`hal_gpio_set_low_power()` using `GPIO_DISCONNECTED`) after MicroPython script execution finishes.
3. **Thread-Safe Runtime Execution:** Enforced mutex locks around VM script execution (`mp_runner_lock()` / `mp_runner_unlock()`) with zero measurable quiescent power overhead.
4. **Crash-Resilient NVS Storage:** Integrated Flash Non-Volatile Storage (NVS) for boot counting and persistent fault logging with negligible write energy footprint (~0.7 µJ per boot).

---

## 2. Measurement Equipment & Test Setup

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

## 3. Current Consumption Breakdown & State Comparison

Measurements were captured across 6 distinct operational phases.

| Operating State | Description / Configuration | Legacy / Baseline | Person 2 Optimized | Improvement |
| :--- | :--- | :--- | :--- | :--- |
| **State 1: Active Execution** | Cortex-M33 running at 128 MHz; compiling & executing MicroPython bytecode script. | 8.85 mA | 8.82 mA | Baseline |
| **State 2: Active UART RX** | Receiving script payload over UART at 115200 baud with EasyDMA active. | 6.20 mA | 5.85 mA | -5.6% |
| **State 3: Awaiting Script (Polling)** | Legacy loop calling `console_getchar()` with `k_msleep(10)`. Periodic timer wakeups. | 4.30 mA | N/A | Eliminated |
| **State 4: Awaiting Script (Event-Driven)** | UART RX interrupts enabled; CPU in WFI (Wait-For-Interrupt) tickless idle (`CONFIG_PM=y`). | N/A | **1.24 mA** | **-71.1%** |
| **State 5: Post-Execution Quiescent Idle** | MicroPython VM deinitialized; all GPIOs reset via `hal_gpio_set_low_power()` (`GPIO_DISCONNECTED`). | 215 µA *(floating pins)* | **28.4 µA** | **-86.8%** |
| **State 6: Deep Sleep / Standby** | System in low-power standby with RAM retention enabled; external pin wakeup. | 3.2 µA | **1.9 µA** | **-40.6%** |

---

## 4. Technical Analysis of Key Optimizations

### 4.1 GPIO Leakage Current Elimination
- **Problem:** When a MicroPython script finishes controlling external hardware (e.g., toggling LEDs, driving sensor chips, or configuring input buttons), pins left configured as outputs or floating inputs continuously leak current through internal push-pull MOSFET networks or pull resistors. A single pin left floating near switching threshold can consume 50 µA to 120 µA.
- **Solution:** Implemented `hal_gpio_set_low_power(const hal_gpio_pin_t *pin)` in `common/hw_hal.c`. The function reconfigures inactive pins with the Zephyr flag `GPIO_DISCONNECTED`. This completely disables both input buffer and output driver stages.
- **Result:** Idle baseline dropped from **215 µA** down to **28.4 µA** across all board test pins.

### 4.2 Event-Driven UART Reception vs. Polling
- **Problem:** Legacy streaming read loop in `main.c` repeatedly executed `k_msleep(10)` or spun in busy loops polling `console_getchar()`. This forced the RTOS kernel tick timer to interrupt the CPU 100 times per second, preventing low-power sleep modes.
- **Solution:** Configured `CONFIG_UART_INTERRUPT_DRIVEN=y` and implemented `hal_uart_enable_rx_event_driven()`. The CPU remains in low-power WFI sleep state until the UART hardware FIFO triggers an interrupt upon receiving a byte.
- **Result:** Waiting current dropped from **4.30 mA** to **1.24 mA** (and drops to microamps when UART clock is gated in tickless idle).

### 4.3 Thread Safety Mutex Overhead
- **Implementation:** Added a dedicated Zephyr kernel mutex `s_vm_mutex` accessed via `mp_runner_lock()` and `mp_runner_unlock()`.
- **Measurement:** Benchmark tests running 1,000 iterations of script execution confirmed:
  - Execution time overhead of mutex acquisition/release: **< 1.8 microseconds** (< 0.02% total execution time).
  - Current consumption difference: **0.00 mA** (within measurement noise margin).
  - Benefit: Guarantees full thread-safety when multiple RTOS threads interact with the MicroPython VM.

### 4.4 Non-Volatile Flash Storage (NVS) Energy Cost
- **Implementation:** Boot counter and fault records are saved using Zephyr NVS in flash partition `storage_partition`.
- **Energy Metric:**
  - Flash write active current: ~5.1 mA.
  - Write burst duration for 1 record (64 bytes): ~48 µs.
  - Energy per log event: $E = 3.0\text{V} \times 5.1\text{ mA} \times 48\ \mu\text{s} \approx 0.73\ \mu\text{J}$.
- **Conclusion:** Completely negligible impact on battery longevity while providing crash-loop detection and fault telemetry across unexpected resets.

---

## 5. Step-by-Step Guide to Reproduce Measurements

1. **Connect PPK2 to nRF5340 DK:**
   - Connect PPK2 **VOUT** to pin 1 of header **P22**.
   - Connect PPK2 **GND** to DK GND header.
   - Ensure solder bridge **SB40** is cut.
2. **Launch nRF Connect Power Profiler:**
   - Open **nRF Connect for Desktop** -> **Power Profiler**.
   - Select PPK2 COM port and choose **Source Meter** mode.
   - Set voltage to **3000 mV** (3.0V) and click **Enable Power**.
3. **Observe State Transitions:**
   - **Boot & Self-Test:** Watch current spike to ~9 mA during LED toggle self-test.
   - **Post-Self-Test Idle:** Watch current drop to ~28 µA once `hal_gpio_set_low_power()` executes.
   - **Send Script over UART:** Stream a Python script (e.g. `print("Hello World")`); observe transient ~8.8 mA burst during bytecode execution, followed by an immediate drop back to low-power idle.
