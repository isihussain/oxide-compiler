# Oxide Compiler Community Launch Kit 🚀

This document contains technical announcement templates for developer, compiler, and systems engineering communities (Hacker News, Reddit, and X/Twitter).

---

## 1. Hacker News (Show HN)

**Title:**  
`Show HN: Oxide – Independent Rust compiler frontend in C++20 with NASA/JPL aerospace verification`

**Link:**  
`https://github.com/isihussain/oxide-compiler`

**Text / Body:**
```markdown
Hi HN,

I built Oxide, an independent implementation of the Rust programming language frontend, type checker, borrow checker, and code generator written from scratch in modern C++20.

GitHub Repository: https://github.com/isihussain/oxide-compiler

Key Architecture & Components:
- Zero LLVM or rustc runtime dependency: compiles source directly to portable C99 target output.
- Non-Lexical Lifetimes (NLL): backward dataflow liveness analysis with loan invalidation.
- Monomorphization pass: reifies generic structs and functions without runtime indirection.
- NASA/JPL Safety Rules Engine (--profile=aerospace):
  * Static loop boundedness analysis (unbounded while-true loops rejected).
  * Complete ban on dynamic heap allocations (malloc/calloc/free).
  * Direct and mutual recursion rejection via call-graph cycle detection.
  * Ariane 5 mitigation: hardware overflow trap emission (__builtin_*_overflow).
- Conformance Test Suite: 155/155 test suites passing across positive language features and negative safety checks.

The full source, test harness, and build scripts are open source under the MIT license. Feedback on compiler passes and borrow-checker invariants is welcome!
```

---

## 2. Reddit (r/rust, r/cpp, r/Compilers)

**Post Title:**  
`Oxide: An independent Rust compiler frontend written in C++20 with NASA/JPL aerospace verification`

**Post Body:**
```markdown
Hello everyone,

I've open-sourced **Oxide**, an independent compiler frontend for Rust written from scratch in C++20 without dependencies on LLVM or rustc.

- **GitHub Repository:** https://github.com/isihussain/oxide-compiler

### Features & Pipeline:
* **Frontend:** Recursive descent parser, lexical scoping, and static type checking.
* **Control Flow Graph (CFG) & NLL:** Lowers AST into basic blocks with explicit terminators and computes variable liveness to enforce non-lexical lifetimes.
* **Aerospace Safety Engine (`--profile=aerospace`):**
  * NASA/JPL Rules 1-3 enforcement (bounded loops, no heap allocation, recursion ban).
  * Sensor Fault Detection, Isolation & Recovery (FDIR) and Watchdog heartbeats.
  * Liquid rocket propulsion guards (turbopump cavitation, actuator slew-rate limits, and dual-condition touchdown interlocks).
* **Test Suite:** 155 unit and negative rejection tests passing on Linux and macOS CI.

Feedback and contributions on AST lowering, borrow checking, and verification algorithms are welcome!
```

---

## 3. X (Twitter) Technical Thread

**Tweet 1:**  
```text
Just open-sourced Oxide: an independent Rust compiler frontend written from scratch in C++20.

Zero LLVM/rustc runtime dependencies — emits clean C99 with full NASA/JPL flight software safety checks.

🔗 https://github.com/isihussain/oxide-compiler
🧵 Technical breakdown below 👇
```

**Tweet 2:**  
```text
Oxide features:
1. Lexer, recursive descent parser, and static typechecker
2. CFG basic block lowering & Non-Lexical Lifetimes (NLL)
3. Monomorphizer for parametric generics
4. Aerospace profile enforcing bounded loops, heap ban, and recursion cycles

155/155 conformance tests passing.
#RustLang #Cpp #Compilers #OpenSource
```
