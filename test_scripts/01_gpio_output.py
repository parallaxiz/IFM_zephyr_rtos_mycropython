"""
Test Case 1: GPIO Output & LED Blink Verification
Target: Nordic nRF5340 DK (LED1 on P0.28) / Native Sim
"""

import time
from machine import Pin

# Port 0 device name mapped from Zephyr devicetree
GPIO0_PORT = "gpio@842500"
LED_PIN = 28  # P0.28 (LED1 on nRF5340 DK)

print("=== [TEST 1] Starting GPIO Output Verification ===")

try:
    # Initialize pin as output
    led = Pin((GPIO0_PORT, LED_PIN), Pin.OUT)
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
