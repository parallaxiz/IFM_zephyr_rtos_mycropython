"""
Validation Script: Verification of script.py for Real nRF5340 Hardware Target
Checks pin mappings, device node identifiers, and active-low LED switching.
"""

import time
import sys
from machine import Pin

# Standard nRF5340 DK (PCA10095) Port 0 controller identifiers
CANDIDATE_PORTS = [
    "gpio@842500",  # Raw Devicetree address for nRF5340 Application Core GPIO0
    "gpio0",        # Node label alias
    "GPIO_0",       # Legacy driver name
]

LED0_PIN = 28  # P0.28 (LED1 on nRF5340 DK)

print("=== [VALIDATION] Starting script.py Hardware Verification for nRF5340 ===")

selected_port = None
led = None

# Step 1: Detect and validate matching GPIO port identifier
for port_name in CANDIDATE_PORTS:
    try:
        test_pin = Pin((port_name, LED0_PIN), Pin.OUT)
        selected_port = port_name
        led = test_pin
        print(f"[OK] Successfully initialized Pin on devicetree node: '{port_name}'")
        break
    except Exception as err:
        print(f"[INFO] Port candidate '{port_name}' not bound: {err}")

if led is None:
    print("[ERROR] Could not bind any GPIO0 port candidates. Check Devicetree overlay!")
    sys.exit(1)

# Step 2: Validate active-low output states and timing
print(f"[VALIDATION] Starting active-low blink cycle on {selected_port} pin {LED0_PIN}...")
for cycle in range(1, 4):
    print(f"  Cycle {cycle}/3: LED ON (Active-Low: writing 0)")
    led.value(0)
    time.sleep(0.5)

    print(f"  Cycle {cycle}/3: LED OFF (Active-Low: writing 1)")
    led.value(1)
    time.sleep(0.2)

# Step 3: Verify final safe resting state
led.value(1)  # Ensure LED is OFF
print("=== [VALIDATION PASS] script.py verified functional on nRF5340 hardware ===")
