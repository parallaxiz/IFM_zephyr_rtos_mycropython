#ifndef MICROPYTHON_RUNNER_H
#define MICROPYTHON_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Executes a Python script from an in-memory C string.
 *
 * This function creates a MicroPython lexer from the source string, parses,
 * compiles, and executes the bytecode inside an NLR (No-Local-Returns) exception block.
 * Any unhandled Python exceptions (e.g. ZeroDivisionError, SyntaxError) are caught,
 * printed via mp_obj_print_exception, and do not crash the Zephyr RTOS thread.
 *
 * @param src Null-terminated C string containing Python source code.
 * @return int 0 on successful execution, -1 if a Python exception or parse error occurred.
 */
int mp_run_string(const char *src);

/**
 * @brief Verification test stub for the MicroPython string runner engine.
 *
 * Tests standard script execution as well as intentional exception handling (e.g., 1 / 0).
 */
void mp_runner_test_stub(void);

#ifdef __cplusplus
}
#endif

#endif /* MICROPYTHON_RUNNER_H */
