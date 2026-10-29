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
        ("examples/01_simple_arithmetic.rs", 42, []),
        ("examples/02_control_flow.rs", 42, []),
        ("examples/03_mutability.rs", 10, []),
        ("examples/04_structs.rs", 42, []),
        ("examples/05_while_loop.rs", 55, []),
        ("examples/06_mutable_struct.rs", 42, []),
        ("examples/07_mutable_reference.rs", 42, []),
        ("examples/08_extern_c_abi.rs", 42, []),
        ("examples/09_aerospace_profile.rs", 42, ["--profile=aerospace"]),
        ("examples/10_tagged_union_enums.rs", 42, []),
        ("examples/11_generics_monomorphization.rs", 42, []),
        ("examples/12_traits_and_impl.rs", 42, []),
    ]

    passed = 0
    total = 0

    for item in positive_tests:
        path_str, expected_exit = item[0], item[1]
        extra_flags = item[2] if len(item) > 2 else []
        total += 1
        src = root / path_str
        bin_out = root / "build" / f"test_{src.stem}"
        cmd = [str(compiler)] + extra_flags + ["-o", str(bin_out), str(src)]
        build_res = subprocess.run(cmd, capture_output=True, text=True)
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
        ("tests/ui/fail_immut_assign.rs", "cannot assign twice to immutable variable", []),
        ("tests/ui/fail_type_mismatch.rs", "mismatched types", []),
        ("tests/ui/fail_uninit.rs", "use of possibly-uninitialized variable", []),
        ("tests/ui/fail_struct_unknown_field.rs", "has no field named", []),
        ("tests/ui/fail_struct_missing_field.rs", "missing field", []),
        ("tests/ui/fail_struct_invalid_field_access.rs", "no field", []),
        ("tests/ui/fail_while_non_bool_condition.rs", "expected 'bool'", []),
        ("tests/ui/fail_assign_immutable_field.rs", "cannot assign to field", []),
        ("tests/ui/fail_deref_immutable_ref_assign.rs", "cannot assign to dereference of immutable reference", []),
        ("tests/ui/fail_extern_type_mismatch.rs", "mismatched type in argument 1", []),
        ("tests/ui/fail_extern_arity_mismatch.rs", "takes 1 arguments but 2 were supplied", []),
        ("tests/ui/fail_aerospace_recursion.rs", "recursion detected along call chain", ["--profile=aerospace"]),
        ("tests/ui/fail_aerospace_mutual_recursion.rs", "recursion detected along call chain", ["--profile=aerospace"]),
        ("tests/ui/fail_aerospace_method_recursion.rs", "recursion detected along call chain", ["--profile=aerospace"]),
        ("tests/ui/fail_match_non_exhaustive.rs", "non-exhaustive patterns: variant 'Blue' not covered", []),
        ("tests/ui/fail_match_arm_type_mismatch.rs", "match arms have incompatible types", []),
        ("tests/ui/fail_enum_payload_mismatch.rs", "mismatched payload type for variant", []),
        ("tests/ui/fail_generic_type_mismatch.rs", "mismatched type in argument 2", []),
        ("tests/ui/fail_generic_struct_field_mismatch.rs", "mismatched type for field 'second'", []),
        ("tests/ui/fail_missing_trait_method.rs", "missing items in 'impl Area for Rectangle': method 'area' not implemented", []),
        ("tests/ui/fail_trait_signature_mismatch.rs", "method 'area' has an incompatible return type", []),
        ("tests/ui/fail_method_not_found.rs", "no method named 'non_existent' found for type 'Point'", []),
        ("tests/ui/fail_immutable_method_call.rs", "cannot borrow immutable local variable 'p' as mutable in method call", []),
    ]

    for item in negative_tests:
        path_str, expected_err_sub = item[0], item[1]
        extra_flags = item[2] if len(item) > 2 else []
        total += 1
        src = root / path_str
        cmd = [str(compiler), "--check"] + extra_flags + [str(src)]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0 and expected_err_sub in res.stderr:
            print(f"[PASS] {src.name} (correctly rejected)")
            passed += 1
        else:
            print(f"[FAIL] {src.name}: expected error containing '{expected_err_sub}', output:\n{res.stderr}")
            print(f"[FAIL] {src.name}: expected error containing '{expected_err_sub}', output:\n{res.stderr}")

    print("=" * 50)
    print(f"Results: {passed}/{total} tests passed.")
    if passed != total:
        sys.exit(1)

if __name__ == "__main__":
    run()
