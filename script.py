import time
from machine import Pin

print("=== Starting Pin Diagnostic Scan ===")

# Try common Zephyr device tree label names and pin ranges
found = False
for port_name in ["gpio0", "gpio1", "GPIO_0", "GPIO_1"]:
    for pin_num in range(32):
        try:
            p = Pin((port_name, pin_num), Pin.OUT)
            print(f"SUCCESS: Found valid pin -> ({port_name}, {pin_num})")
            p.value(0)
            time.sleep(0.1)
            p.value(1)
            found = True
            break
        except Exception as e:
            pass
    if found:
        break

if not found:
    print("Trying absolute integer pins...")
    for pin_num in range(64):
        try:
            p = Pin(pin_num, Pin.OUT)
            print(f"SUCCESS: Found absolute pin -> {pin_num}")
            p.value(0)
            time.sleep(0.1)
            p.value(1)
        except Exception:
            pass

print("=== Scan Finished ===")