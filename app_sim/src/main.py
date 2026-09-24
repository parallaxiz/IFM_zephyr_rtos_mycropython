import time
from machine import Pin

# LED pin on nRF5340 DK (typically pin 13 or designated user LED GPIO)
led = Pin(13, Pin.OUT)

print("[HARDWARE] Starting LED blink...")
for cycle in range(1, 5):
    print(f"[HARDWARE] Cycle {cycle}/4: ON")
    led.on()
    time.sleep_ms(1000)

    print(f"[HARDWARE] Cycle {cycle}/4: OFF")
    led.off()
    time.sleep_ms(1000)

print("[HARDWARE] Done!")