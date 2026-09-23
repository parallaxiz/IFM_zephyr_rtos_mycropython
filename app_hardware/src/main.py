# MicroPython Hardware Application Script
# This file is executed directly on your physical target board.
import time

print("[HARDWARE MAIN] MicroPython hardware runtime initialized.")
print("[HARDWARE MAIN] Starting hardware execution loop...")

for cycle in range(1, 6):
    print("[HARDWARE MAIN] Board heartbeat cycle %d/5" % cycle)
    time.sleep(1)

print("[HARDWARE MAIN] Hardware sequence completed successfully!")
