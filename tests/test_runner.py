#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

def run():
    root = Path(__file__).resolve().parent.parent
    compiler = root / "bin" / "oxide"

    if not compiler.exists():
        print(f"Error: Compiler binary not found at {compiler}")
        sys.exit(1)

    print("Running Oxide Conformance Test Suite...")
    print("=" * 50)

    # Positive Tests (Execution & Result Check)
    positive_tests = [
        ("examples/01_simple_arithmetic.rs", 42),
        ("examples/02_control_flow.rs", 42),
        ("examples/03_mutability.rs", 10),
    ]

    passed = 0
    total = 0

    for path_str, expected_exit in positive_tests:
        total += 1
        src = root / path_str
        bin_out = root / "build" / f"test_{src.stem}"
        build_res = subprocess.run([str(compiler), "-o", str(bin_out), str(src)], capture_output=True, text=True)
        if build_res.returncode != 0:
            print(f"[FAIL] {src.name}: compilation failed\n{build_res.stderr}")
            continue

        run_res = subprocess.run([str(bin_out)], capture_output=True)
        if run_res.returncode == expected_exit:
            print(f"[PASS] {src.name} (exit code: {run_res.returncode})")
            passed += 1
        else:
            print(f"[FAIL] {src.name}: expected exit {expected_exit}, got {run_res.returncode}")

    # Negative Tests (Expected compilation failures)
    negative_tests = [
        ("tests/ui/fail_immut_assign.rs", "cannot assign twice to immutable variable"),
        ("tests/ui/fail_type_mismatch.rs", "mismatched types"),
        ("tests/ui/fail_uninit.rs", "use of possibly-uninitialized variable"),
    ]

    for path_str, expected_err_sub in negative_tests:
        total += 1
        src = root / path_str
        res = subprocess.run([str(compiler), "--check", str(src)], capture_output=True, text=True)
        if res.returncode != 0 and expected_err_sub in res.stderr:
            print(f"[PASS] {src.name} (correctly rejected)")
            passed += 1
        else:
            print(f"[FAIL] {src.name}: expected error containing '{expected_err_sub}', output:\n{res.stderr}")

    print("=" * 50)
    print(f"Results: {passed}/{total} tests passed.")
    if passed != total:
        sys.exit(1)

if __name__ == "__main__":
    run()
