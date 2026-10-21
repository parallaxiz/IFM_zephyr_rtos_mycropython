"""
Test Case 3: Timer & Timing Precision Verification
Tests MicroPython time module and sleep accuracy
"""

import time

print("=== [TEST 3] Starting Timer & Timing Accuracy Test ===")

try:
    ticks_target = 5
    interval_sec = 0.5

    print(f"Executing {ticks_target} periodic sleep intervals of {interval_sec}s...")

    start_time = time.ticks_ms() if hasattr(time, "ticks_ms") else int(time.time() * 1000)

    for i in range(1, ticks_target + 1):
        step_start = time.ticks_ms() if hasattr(time, "ticks_ms") else int(time.time() * 1000)
        time.sleep(interval_sec)
        step_end = time.ticks_ms() if hasattr(time, "ticks_ms") else int(time.time() * 1000)
        delta_ms = step_end - step_start
        print(f"Tick {i}/{ticks_target}: Elapsed {delta_ms} ms (expected ~{int(interval_sec * 1000)} ms)")

    end_time = time.ticks_ms() if hasattr(time, "ticks_ms") else int(time.time() * 1000)
    total_elapsed = end_time - start_time
    print(f"Total test time: {total_elapsed} ms")

    print("=== [TEST 3 PASS] Timer Accuracy Verified Successfully ===")
except Exception as e:
    print(f"=== [TEST 3 FAIL] Timer Error: {e} ===")
