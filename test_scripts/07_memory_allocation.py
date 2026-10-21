"""
Test Case 7: Heap Allocation, Memory Stress & Garbage Collector
Verifies heap allocation limits and gc.collect() reclaiming
"""

import gc

print("=== [TEST 7] Starting Heap Allocation & Garbage Collection Test ===")

try:
    gc.collect()
    free_start = gc.mem_free()
    alloc_start = gc.mem_alloc()
    print(f"Initial Heap State: Alloc={alloc_start} bytes, Free={free_start} bytes")

    # Allocate 1000 items in dynamic list
    print("Allocating 1,000 integer elements in dynamic list...")
    data_buffer = [i * 2 for i in range(1000)]
    alloc_mid = gc.mem_alloc()
    free_mid = gc.mem_free()
    print(f"Mid-Test Heap State: Alloc={alloc_mid} bytes, Free={free_mid} bytes")

    # Delete buffer and trigger garbage collection
    print("Deleting buffer and invoking gc.collect()...")
    del data_buffer
    gc.collect()

    free_end = gc.mem_free()
    alloc_end = gc.mem_alloc()
    print(f"Post-GC Heap State: Alloc={alloc_end} bytes, Free={free_end} bytes")

    reclaimed = free_end - free_mid
    print(f"Memory reclaimed by GC: {reclaimed} bytes")
    assert reclaimed > 0, "Garbage collection failed to reclaim memory"

    print("=== [TEST 7 PASS] Heap Memory & GC Validated Successfully ===")
except Exception as e:
    print(f"=== [TEST 7 FAIL] Memory Stress Test Error: {e} ===")
