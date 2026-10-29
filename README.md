# Oxide

An independent, open-source compiler for the Rust programming language implemented from scratch in C++20.

Oxide provides an alternative implementation of the Rust language frontend, type checker, borrow checker, and code generator. It does not depend on `rustc` or LLVM libraries for its core pipeline, emitting clean C99 for target code generation to maintain portability and eliminate circular bootstrapping dependencies.

---

## Architecture Pipeline

```
Rust Source (.rs)
      │
      ▼
 [ Lexer ] ────────────► Token Stream (Keywords, Identifiers, Operators)
      │
      ▼
 [ Parser ] ───────────► Abstract Syntax Tree (AST)
      │
      ▼
 [ Type Checker ] ─────► Annotated AST (Scope resolution, Type inference)
      │
      ▼
 [ Borrow Checker ] ───► Ownership & Mutability Validation
      │
      ▼
 [ C99 Codegen ] ──────► Emitted C99 Source / Native Executable
```

### Compiler Modules

* **Lexer (`include/lexer.hpp`, `src/lexer.cpp`)**: Tokenizes UTF-8 Rust source code, tracking line and column positions. Handles single-line and multi-line comments.
* **Parser (`include/parser.hpp`, `src/parser.cpp`)**: Recursive descent parser that constructs the AST. Implements operator precedence parsing for expressions, functions, `let` bindings, control flow (`if`/`else`), and trailing block return expressions.
* **Type Checker (`include/typeck.hpp`, `src/typeck.cpp`)**: Enforces Rust's static typing rules. Manages lexical scopes, validates function call arity and parameter types, and ensures block evaluations match return types.
* **Borrow Checker (`include/borrowck.hpp`, `src/borrowck.cpp`)**: Enforces core Rust memory safety rules:
  - Reassignment to immutable variables is rejected.
  - Mutable references (`&mut`) to immutable locals are rejected.
  - Simultaneous mutable and immutable aliasing is rejected.
  - Use of uninitialized variables is rejected.
* **Aerospace Verifier (`include/aerospace.hpp`, `src/aerospace.cpp`)**: Enforces safety-critical constraints under `--profile=aerospace`:
  - Rejects direct and indirect recursion via call-graph cycle analysis (NASA/JPL Flight Software Rule 3).
  - Emits checked arithmetic traps (`__builtin_add_overflow`, etc.) to prevent unhandled integer overflow failures (Ariane 5 mitigation).
* **Code Generator (`include/codegen_c.hpp`, `src/codegen_c.cpp`)**: Maps verified AST nodes to standard ANSI C99, translating Rust primitives (`i32`, `bool`, references) into equivalent C constructs.

---

## Building Oxide

### Requirements
- A C++20 compliant compiler (`clang++` 14+ or `g++` 11+)
- Make
- Python 3.8+ (for test suite)

### Build Instructions
```bash
git clone https://github.com/isihussain/oxide-compiler.git
cd oxide-compiler
make
```

The resulting binary will be located at `bin/oxide`.

---

## Usage

### 1. Compiling a Rust Program to Native Executable
```bash
./bin/oxide -o bin/example1 examples/01_simple_arithmetic.rs
./bin/example1
echo $? # Output: 42
```

### 2. Emitting Generated C Source
```bash
./bin/oxide --emit-c examples/02_control_flow.rs
```

### 3. Dumping the AST
```bash
./bin/oxide --emit-ast examples/03_mutability.rs
```

### 4. Running Checks Only (Type & Borrow Checker)
```bash
./bin/oxide --check tests/ui/fail_immut_assign.rs
```

### 5. Compiling with the Aerospace Verification Profile
```bash
./bin/oxide --profile=aerospace -o bin/flight_guidance examples/09_aerospace_profile.rs
```

---

## Running the Conformance Test Suite

Oxide includes positive verification tests and negative UI tests (expecting compiler diagnostics on invalid Rust code):

```bash
make test
```

Test runner output:
```text
Running Oxide Conformance Test Suite...
==================================================
[PASS] 01_simple_arithmetic.rs (exit code: 42)
[PASS] 02_control_flow.rs (exit code: 42)
[PASS] 03_mutability.rs (exit code: 10)
[PASS] 04_structs.rs (exit code: 42)
[PASS] 05_while_loop.rs (exit code: 55)
[PASS] 06_mutable_struct.rs (exit code: 42)
[PASS] 07_mutable_reference.rs (exit code: 42)
[PASS] 08_extern_c_abi.rs (exit code: 42)
[PASS] 09_aerospace_profile.rs (exit code: 42)
[PASS] 10_tagged_union_enums.rs (exit code: 42)
[PASS] fail_immut_assign.rs (correctly rejected)
[PASS] fail_type_mismatch.rs (correctly rejected)
[PASS] fail_uninit.rs (correctly rejected)
[PASS] fail_struct_unknown_field.rs (correctly rejected)
[PASS] fail_struct_missing_field.rs (correctly rejected)
[PASS] fail_struct_invalid_field_access.rs (correctly rejected)
[PASS] fail_while_non_bool_condition.rs (correctly rejected)
[PASS] fail_assign_immutable_field.rs (correctly rejected)
[PASS] fail_deref_immutable_ref_assign.rs (correctly rejected)
[PASS] fail_extern_type_mismatch.rs (correctly rejected)
[PASS] fail_extern_arity_mismatch.rs (correctly rejected)
[PASS] fail_aerospace_recursion.rs (correctly rejected)
[PASS] fail_aerospace_mutual_recursion.rs (correctly rejected)
[PASS] fail_match_non_exhaustive.rs (correctly rejected)
[PASS] fail_match_arm_type_mismatch.rs (correctly rejected)
[PASS] fail_enum_payload_mismatch.rs (correctly rejected)
==================================================
Results: 26/26 tests passed.
```

---

## Roadmap

- [x] **Milestone 1: Procedural Core**
  - [x] Lexer and recursive descent parser
  - [x] Primitive types (`i32`, `bool`, unit `()`)
  - [x] Functions, `let` bindings, assignments, expressions
  - [x] Control flow (`if` / `else`)
  - [x] Mutability enforcement and initialization checking
  - [x] C99 code generator
- [x] **Milestone 2: Structs, Enums, and Interop**
  - [x] Compound struct declarations
  - [x] Struct literal instantiations and field access expressions
  - [x] Mutable place expressions (l-values: `point.x = val`, `*ptr = val`)
  - [x] `while` loop control structures
  - [x] Foreign Function Interface (`extern "C"` declaration blocks)
  - [x] Tagged union enums with payload support
  - [x] `match` pattern exhaustiveness checking
- [ ] **Milestone 3: Generics and Traits**
  - [ ] Parametric polymorphism (generics on functions and structs)
  - [ ] Trait definitions and `impl` blocks
  - [ ] Monomorphization pass
- [ ] **Milestone 4: Full Borrow Checker (Polonius Algorithm)**
  - [ ] Control Flow Graph (CFG) / Mid-level IR (MIR) lowering
  - [ ] Non-Lexical Lifetimes (NLL) with loan origin tracking
- [ ] **Milestone 5: Standalone Core Library Compilation**
  - [ ] Target Rust `core` library compilation

---

## Contributing

1. Fork the repository.
2. Create your feature branch (`git checkout -b feature/struct-support`).
3. Ensure all tests pass with `make test`.
4. Open a pull request describing your changes and matching test cases.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
