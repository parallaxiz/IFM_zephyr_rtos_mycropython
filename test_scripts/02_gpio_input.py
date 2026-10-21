"""
Test Case 2: GPIO Input & Pull-Up Configuration
Target: Nordic nRF5340 DK (Button 1 on P0.23) / Native Sim
"""

import time
from machine import Pin

GPIO0_PORT = "gpio@842500"
BUTTON_PIN = 23  # P0.23 (Button 1 on nRF5340 DK, Active-Low)

print("=== [TEST 2] Starting GPIO Input Verification ===")

try:
    # Initialize pin as input with internal pull-up resistor
    btn = Pin((GPIO0_PORT, BUTTON_PIN), Pin.IN, Pin.PULL_UP)
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
