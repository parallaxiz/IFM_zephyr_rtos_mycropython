import simulator

print("[PYTHON MAIN] Starting Python-controlled LED simulation from main.py...")

for cycle in range(1, 5):
    print("[PYTHON MAIN] Blink cycle %d/4: Turning LED ON" % cycle)
    simulator.set_led(True)
    simulator.sleep_ms(1000)

    print("[PYTHON MAIN] Blink cycle %d/4: Turning LED OFF" % cycle)
    simulator.set_led(False)
    simulator.sleep_ms(1000)

print("[PYTHON MAIN] Simulation completed successfully!")
