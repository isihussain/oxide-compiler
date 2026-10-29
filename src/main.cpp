#include "ast.hpp"
#include "borrowck.hpp"
#include "codegen_c.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "typeck.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void print_usage(const char* prog) {
    std::cout << "Oxide - Independent Rust Compiler (Milestone 1 Core)\n"
              << "Usage: " << prog << " [options] <input.rs>\n\n"
              << "Options:\n"
              << "  -o <file>       Compile to executable binary (via host C toolchain)\n"
              << "  --emit-ast      Dump the Abstract Syntax Tree (AST)\n"
              << "  --emit-c        Output generated C99 code\n"
              << "  --check         Run type checking and borrow checking only\n"
              << "  -v, --version   Display compiler version\n"
              << "  -h, --help      Display this help message\n";
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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "oxide 0.1.0 (independent rust compiler, milestone 1)\n";
            return 0;
        } else if (arg == "--emit-ast") {
            emit_ast = true;
        } else if (arg == "--emit-c") {
            emit_c = true;
        } else if (arg == "--check") {
            check_only = true;
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

    // 4. Borrow Checking
    oxide::BorrowChecker borrowck(*program);
    if (!borrowck.check()) {
        for (const auto& err : borrowck.errors()) {
            std::cerr << "oxide: " << err << "\n";
        }
        return 1;
    }

    if (emit_ast) {
        program->dump();
        return 0;
    }

    if (check_only) {
        std::cout << "oxide: check completed successfully (0 errors).\n";
        return 0;
    }

    // 5. Code Generation (C99)
    oxide::CCodeGenerator codegen(*program);
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
        std::string compile_cmd = "clang -std=c99 -O2 \"" + c_file_path + "\" -o \"" + output_bin + "\"";
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
