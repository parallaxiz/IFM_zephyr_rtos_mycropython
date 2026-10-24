"""
Test Case 1: GPIO Output & LED Blink Verification
Target: Nordic nRF5340 DK (LED1 on P0.28) / Native Sim
"""

import time
from machine import Pin

# Candidate targets: (device_name, pin_number)
# 1. Nordic nRF5340 DK: P0.28 (LED1 on gpio@842500)
# 2. Native Sim: Pin 0 (LED0 on gpio_emul)
CANDIDATES = [
    ("gpio@842500", 28),
    ("gpio_emul", 0),
]

print("=== [TEST 1] Starting GPIO Output Verification ===")

try:
    led = None
    for port, pin in CANDIDATES:
        try:
            led = Pin((port, pin), Pin.OUT)
            GPIO0_PORT, LED_PIN = port, pin
            break
        except ValueError:
            continue

    if led is None:
        raise ValueError("no compatible GPIO device found")

    print(f"Configured {GPIO0_PORT} pin {LED_PIN} as OUTPUT")

    # Cycle pin through active-low (ON) and inactive (OFF) states
    for cycle in range(1, 4):
        print(f"Cycle {cycle}/3: LED ON (Drive 0)")
        led.value(0)
        time.sleep(0.5)

        print(f"Cycle {cycle}/3: LED OFF (Drive 1)")
        led.value(1)
        time.sleep(0.5)

    print("=== [TEST 1 PASS] GPIO Output Verified Successfully ===")
except Exception as e:
    print(f"=== [TEST 1 FAIL] GPIO Output Error: {e} ===")
