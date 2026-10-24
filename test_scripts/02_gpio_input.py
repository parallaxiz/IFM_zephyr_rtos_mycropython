"""
Test Case 2: GPIO Input & Pull-Up Configuration
Target: Nordic nRF5340 DK (Button 1 on P0.23) / Native Sim
"""

import time
from machine import Pin

# Candidate targets: (device_name, pin_number)
# 1. Nordic nRF5340 DK: P0.23 (Button 1 on gpio@842500, Active-Low)
# 2. Native Sim: Pin 1 (Button 1 on gpio_emul)
CANDIDATES = [
    ("gpio@842500", 23),
    ("gpio_emul", 1),
]

print("=== [TEST 2] Starting GPIO Input Verification ===")

try:
    btn = None
    for port, pin in CANDIDATES:
        try:
            btn = Pin((port, pin), Pin.IN, Pin.PULL_UP)
            GPIO0_PORT, BUTTON_PIN = port, pin
            break
        except ValueError:
            continue

    if btn is None:
        raise ValueError("no compatible GPIO input device found")

    print(f"Configured {GPIO0_PORT} pin {BUTTON_PIN} as INPUT with PULL_UP")

    # Sample digital input level 5 times
    for sample in range(1, 6):
        val = btn.value()
        state = "PRESSED (LOW)" if val == 0 else "RELEASED (HIGH)"
        print(f"Sample {sample}/5: Pin {BUTTON_PIN} read = {val} ({state})")
        time.sleep(0.2)

    print("=== [TEST 2 PASS] GPIO Input Verified Successfully ===")
except Exception as e:
    print(f"=== [TEST 2 FAIL] GPIO Input Error: {e} ===")
