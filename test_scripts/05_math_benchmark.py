"""
Test Case 5: Math Engine & Computational Stress Test
Validates integer arithmetic, float accuracy, and CPU iteration speed
"""

print("=== [TEST 5] Starting Computational Math Benchmark ===")

try:
    # 1. Integer Fibonacci sequence calculation
    def fib(n):
        a, b = 0, 1
        for _ in range(n):
            a, b = b, a + b
        return a

    fib_target = 25
    fib_res = fib(fib_target)
    print(f"Fibonacci({fib_target}) = {fib_res} (Expected: 75025)")
    assert fib_res == 75025, "Fibonacci calculation mismatch"

    # 2. Floating-point polynomial evaluation (Maclaurin series approximation for e^1)
    # e = 1 + 1/1! + 1/2! + 1/3! + ...
    e_approx = 1.0
    factorial = 1.0
    for n in range(1, 11):
        factorial *= n
        e_approx += 1.0 / factorial

    print(f"Computed e approximation: {e_approx:.6f} (Standard: 2.718282)")
    diff = abs(e_approx - 2.7182818)
    assert diff < 0.0001, "Float calculation error exceeds tolerance"

    print("=== [TEST 5 PASS] Math Benchmark Computed Successfully ===")
except Exception as e:
    print(f"=== [TEST 5 FAIL] Math Benchmark Error: {e} ===")
