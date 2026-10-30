#include "aerospace.hpp"
#include "ast.hpp"
#include "borrowck.hpp"
#include "codegen_c.hpp"
#include "lexer.hpp"
#include "monomorphize.hpp"
#include "optimizer.hpp"
#include "parser.hpp"
#include "typeck.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

/**
 * @file main.cpp
 * @brief Oxide compiler driver and CLI entry point.
 *
 * Orchestrates the sequential compiler phases:
 *   Phase 1: Lexical analysis (UTF-8 source tokenization)
 *   Phase 2: Recursive-descent AST parsing and operator precedence climbing
 *   Phase 3: Static type checking, trait checking, and exhaustiveness analysis
 *   Phase 4: Monomorphization (generic function and struct specialization)
 *   Phase 5: Control flow graph generation and Non-Lexical Lifetimes borrow checking
 *   Phase 6: Aerospace safety-critical verification (NASA/JPL rules 1, 2, 3)
 *   Phase 7: AST-level optimization (constant folding and dead code elimination)
 *   Phase 8: ANSI C99 code generation and host C toolchain compilation
 */

void print_usage(const char* prog) {
    std::cout << "Oxide - Independent Rust Compiler with Aerospace Safety Engine\n"
              << "Usage: " << prog << " [options] <input.rs>\n\n"
              << "Options:\n"
              << "  -o <file>             Compile to executable binary (via host C toolchain)\n"
              << "  --emit-ast            Dump the Abstract Syntax Tree (AST)\n"
              << "  --emit-c              Output generated C99 code\n"
              << "  --check               Run type checking and borrow checking only\n"
              << "  --profile=aerospace   Enable safety-critical verification and checked arithmetic\n"
              << "  -v, --version         Display compiler version\n"
              << "  -h, --help            Display this help message\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string input_path;
    std::string output_bin;
    bool emit_ast = false;
    bool emit_c = false;
    bool check_only = false;
    bool aerospace_profile = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "oxide 0.6.0 (independent rust compiler, aerospace edition)\n";
            return 0;
        } else if (arg == "--emit-ast") {
            emit_ast = true;
        } else if (arg == "--emit-c") {
            emit_c = true;
        } else if (arg == "--check") {
            check_only = true;
        } else if (arg == "--profile=aerospace") {
            aerospace_profile = true;
        } else if (arg == "-o") {
            if (i + 1 < argc) {
                output_bin = argv[++i];
            } else {
                std::cerr << "oxide: error: missing output filename after '-o'\n";
                return 1;
            }
        } else if (arg.rfind("-", 0) == 0) {
            std::cerr << "oxide: error: unrecognized option: " << arg << "\n";
            return 1;
        } else {
            input_path = arg;
        }
    }

    if (input_path.empty()) {
        std::cerr << "oxide: error: no input files provided\n";
        return 1;
    }

    std::ifstream in(input_path);
    if (!in.is_open()) {
        std::cerr << "oxide: error: failed to open input file: " << input_path << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();

    // 1. Lexing
    oxide::Lexer lexer(source, input_path);
    std::vector<oxide::Token> tokens;
    try {
        tokens = lexer.tokenize();
    } catch (const std::exception& e) {
        std::cerr << "oxide: lexical error: " << e.what() << "\n";
        return 1;
    }

    // 2. Parsing
    oxide::Parser parser(std::move(tokens));
    std::unique_ptr<oxide::Program> program;
    try {
        program = parser.parse_program();
    } catch (const std::exception& e) {
        std::cerr << "oxide: " << e.what() << "\n";
        return 1;
    }

    // 3. Type Checking
    oxide::TypeChecker typeck(*program);
    if (!typeck.check()) {
        for (const auto& err : typeck.errors()) {
            std::cerr << "oxide: " << err << "\n";
        }
        return 1;
    }

    // 4. Monomorphization Pass (compile-time generic specialization)
    oxide::Monomorphizer mono(*program);
    mono.run();

    // 5. Borrow Checking
    oxide::BorrowChecker borrowck(*program);
    if (!borrowck.check()) {
        for (const auto& err : borrowck.errors()) {
            std::cerr << "oxide: " << err << "\n";
        }
        return 1;
    }

    // 6. Aerospace Safety-Critical Verification (NASA/JPL Rule 3: bounded recursion)
    if (aerospace_profile) {
        oxide::AerospaceVerifier aero(*program);
        if (!aero.verify()) {
            for (const auto& err : aero.errors()) {
                std::cerr << "oxide: " << err << "\n";
            }
            return 1;
        }
    }

    // 7. AST Optimization (Constant Folding & Dead Code Elimination)
    oxide::Optimizer opt(*program);
    opt.run();

    if (emit_ast) {
        program->dump();
        return 0;
    }

    if (check_only) {
        std::cout << "oxide: check completed successfully (0 errors).\n";
        return 0;
    }

    // 8. Code Generation (C99)
    oxide::CCodeGenerator codegen(*program, aerospace_profile);
    std::string c_code = codegen.generate();

    if (emit_c && output_bin.empty()) {
        std::cout << c_code;
        return 0;
    }

    // Write temporary C file or user output
    std::string c_file_path = output_bin.empty() ? "a.out.c" : (output_bin + ".c");
    std::ofstream c_out(c_file_path);
    if (!c_out.is_open()) {
        std::cerr << "oxide: error: cannot write output C file: " << c_file_path << "\n";
        return 1;
    }
    c_out << c_code;
    c_out.close();

    if (!output_bin.empty()) {
        std::string compile_cmd = "clang -std=c99 -O2 \"" + c_file_path + "\" -o \"" + output_bin + "\" -lm";
        int res = std::system(compile_cmd.c_str());
        if (res != 0) {
            std::cerr << "oxide: backend compilation error (exit code " << res << ")\n";
            return 1;
        }
        std::cout << "oxide: built binary '" << output_bin << "' successfully.\n";
    } else {
        std::cout << "oxide: emitted C source to '" << c_file_path << "'.\n";
    }

    return 0;
}
