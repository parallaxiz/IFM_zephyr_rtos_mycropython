import time
from machine import Pin

try:
    led = Pin(("GPIO_0", 13), Pin.OUT)
    print("[HARDWARE] Starting LED blink...")
    for cycle in range(1, 5):
        print(f"[HARDWARE] Cycle {cycle}/4: ON")
        led.on()
        time.sleep_ms(200)

        print(f"[HARDWARE] Cycle {cycle}/4: OFF")
        led.off()
        time.sleep_ms(200)

    print("[HARDWARE] Done!")
except Exception as e:
    print(f"[SIMULATOR] Note: GPIO controller not bound on native_sim ({e}). Continuing.")