#!/usr/bin/env python3
"""
test_scripts/validate_script.py — Static AST-based Security & Syntax Validator
for MicroPython scripts targeting Zephyr RTOS on nRF5340.

Uses Python's `ast` module (ast.parse and ast.NodeVisitor) to analyze Python
scripts before transmission over UART:
  - Verifies Python syntax validity.
  - Enforces payload size limits (<= 6000 bytes).
  - Enforces strict module import whitelist: {machine, time, utime, math, gc, hw_hal}.
  - Explicitly detects and rejects dangerous imports (os, socket, uctypes, etc.)
    with precise line numbers and column offsets.
"""

import sys
import ast
import argparse
from pathlib import Path

# Strict module import whitelist allowed on embedded MicroPython firmware
ALLOWED_MODULES = {
    "machine",
    "time",
    "utime",
    "math",
    "gc",
    "hw_hal",
}

# Maximum allowed script size in bytes (matching firmware UART_PROTO_MAX_PAYLOAD)
MAX_SCRIPT_SIZE = 6000


class SecurityImportValidator(ast.NodeVisitor):
    """
    AST visitor that inspects all import statements against the security whitelist.
    """

    def __init__(self, filename: str):
        self.filename = filename
        self.violations = []

    def _check_module(self, mod_name: str, lineno: int, col_offset: int):
        root_module = mod_name.split(".")[0]
        if root_module not in ALLOWED_MODULES:
            self.violations.append({
                "module": mod_name,
                "root_module": root_module,
                "lineno": lineno,
                "col_offset": col_offset,
            })

    def visit_Import(self, node: ast.Import):
        for alias in node.names:
            self._check_module(alias.name, node.lineno, node.col_offset)
        self.generic_visit(node)

    def visit_ImportFrom(self, node: ast.ImportFrom):
        if node.module:
            self._check_module(node.module, node.lineno, node.col_offset)
        else:
            self.violations.append({
                "module": "<relative_import>",
                "root_module": "<relative>",
                "lineno": node.lineno,
                "col_offset": node.col_offset,
            })
        self.generic_visit(node)


def validate_script(script_path: str) -> bool:
    path = Path(script_path)
    if not path.is_file():
        print(f"[ERROR] Script file not found: {script_path}")
        return False

    try:
        content = path.read_text(encoding="utf-8")
        byte_len = len(content.encode("utf-8"))
    except UnicodeDecodeError as e:
        print(f"[ERROR] Failed to decode {script_path} as UTF-8: {e}")
        return False

    print(f"=== [VALIDATING] {script_path} ({byte_len} bytes) ===")

    # 1. Size constraint check
    if byte_len == 0:
        print("[ERROR] Script is empty (0 bytes).")
        return False

    if byte_len > MAX_SCRIPT_SIZE:
        print(
            f"[ERROR] Script size ({byte_len} bytes) exceeds firmware limit of "
            f"{MAX_SCRIPT_SIZE} bytes."
        )
        return False

    # 2. Syntax validation via AST parse
    try:
        tree = ast.parse(content, filename=str(path))
    except SyntaxError as syn_err:
        print(
            f"[SYNTAX ERROR] Line {syn_err.lineno}, col {syn_err.offset}: "
            f"{syn_err.msg}"
        )
        if syn_err.text:
            print(f"  --> {syn_err.text.strip()}")
        return False

    # 3. Security import analysis via NodeVisitor
    visitor = SecurityImportValidator(str(path))
    visitor.visit(tree)

    if visitor.violations:
        print(f"[SECURITY REJECTION] {len(visitor.violations)} disallowed import(s) detected:")
        for v in visitor.violations:
            print(
                f"  Line {v['lineno']}:{v['col_offset']} - Disallowed module '{v['module']}' "
                f"(Allowed whitelist: {', '.join(sorted(ALLOWED_MODULES))})"
            )
        return False

    print("[VALIDATION PASS] Script syntax valid and all imports conform to security whitelist.")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Static AST Security & Syntax Validator for Zephyr MicroPython scripts."
    )
    parser.add_argument(
        "script",
        nargs="?",
        default="script.py",
        help="Path to Python script to validate (default: script.py)",
    )
    args = parser.parse_args()

    success = validate_script(args.script)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
