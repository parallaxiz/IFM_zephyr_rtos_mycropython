# Precompiled Firmware for Nordic nRF5340 DK

This directory contains the verified, ready-to-flash binary for the **Nordic Semiconductor nRF5340 DK (PCA10095)** application core (`nrf5340dk/nrf5340/cpuapp`).

## Quick Flashing (No Zephyr / SDK Build Environment Required)

Connect the nRF5340 DK board via the **J2 Micro-USB (Debug / VCOM)** port to your computer.

### Using Nordic Command Line Tools (`nrfjprog`)
```bash
nrfjprog --program firmware/nrf5340dk_zephyr.hex --chiperase --reset -f NRF53 --coprocessor CP_APPLICATION
```

### Using West (If you have Zephyr installed)
```bash
west flash --hex-file firmware/nrf5340dk_zephyr.hex
```

---

## Running Python Scripts over USB

1. Install pyserial:
   ```bash
   pip install pyserial
   ```

2. Identify the COM port:
   * **Windows:** Device Manager -> Ports (COM & LPT) -> `JLink CDC UART Port` (e.g. `COM5`).
   * **Linux:** `/dev/ttyACM0`

3. Transmit a Python script:
   ```bash
   python tools/send_script.py COM5 script.py
   ```

4. Run hardware test scripts:
   * **Blink LED 1 (P0.28):**
     ```bash
     python tools/send_script.py COM5 test_scripts/01_gpio_output.py
     ```
   * **Sample Button 1 (P0.23):**
     ```bash
     python tools/send_script.py COM5 test_scripts/02_gpio_input.py
     ```
