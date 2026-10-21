"""
Test Case 6: Python Exception Handling & NLR Safety
Verifies that runtime exceptions (ZeroDivisionError, IndexError, KeyError)
are properly caught and do not cause Zephyr kernel crash/reboot.
"""

print("=== [TEST 6] Starting Exception Handling Safety Verification ===")

passed_checks = 0

# Check 1: ZeroDivisionError handling
try:
    print("Testing intentional division by zero...")
    val = 42 / 0
    print("ERROR: Division by zero did not trigger exception!")
except ZeroDivisionError as err:
    print(f"Successfully caught ZeroDivisionError: {err}")
    passed_checks += 1

# Check 2: IndexError handling
try:
    print("Testing intentional list out-of-bounds...")
    sample_list = [10, 20, 30]
    out_of_bounds = sample_list[99]
    print("ERROR: Out-of-bounds did not trigger exception!")
except IndexError as err:
    print(f"Successfully caught IndexError: {err}")
    passed_checks += 1

# Check 3: ValueError handling
try:
    print("Testing intentional ValueError parsing...")
    bad_int = int("not_a_number_1234")
    print("ERROR: Bad integer parse did not trigger exception!")
except ValueError as err:
    print(f"Successfully caught ValueError: {err}")
    passed_checks += 1

if passed_checks == 3:
    print("=== [TEST 6 PASS] All 3 Exception Scenarios Caught Safely ===")
else:
    print(f"=== [TEST 6 FAIL] Only {passed_checks}/3 checks passed ===")
