#pragma once

#include "ast.hpp"
#include <sstream>
#include <string>

namespace oxide {

/**
 * @brief Backend code generator emitting standard ANSI/ISO C99 source code.
 *
 * Translates verified Oxide AST constructs into C99 equivalents:
 *  - Primitive types: 'i32' -> 'int32_t', 'bool' -> 'bool', '&str' -> 'const char*'.
 *  - Stack arrays: '[T; N]' -> 'T var[N]' with bounds verification traps.
 *  - Tagged union enums: C enum tag plus union payload struct and inline constructor functions.
 *  - Pattern matching: Switch statement on discriminant tag extracting payload union fields.
 *  - Built-in macros: 'println!' -> 'printf', 'assert!' -> 'assert', 'panic!' -> 'fprintf + abort'.
 *  - Aerospace profile: Injects compiler built-ins '__builtin_add_overflow', '__builtin_sub_overflow',
 *    and '__builtin_mul_overflow' to trap on integer overflow at runtime.
 */
class CCodeGenerator {
public:
    explicit CCodeGenerator(const Program& program, bool aerospace_profile = false);
    std::string generate();

private:
    void emit_includes();
    void emit_aerospace_traps();
    void emit_struct_declarations();
    void emit_enum_declarations();
    void emit_enum_constructors();
    void emit_forward_declarations();
    void emit_function(const Function& func);
    void emit_block(const BlockExpr& block, bool is_function_body);
    void emit_statement(const Statement& stmt);
    void emit_expression(const Expression& expr);
    void emit_match_arm_body(const Expression& body, const std::string& res_var, bool has_res);
    std::string map_type(const Type& type);

    const Program& program_;
    bool aerospace_profile_ = false;
    std::stringstream out_;
    int indent_level_ = 0;
    int match_counter_ = 0;

    void indent();
    void line(const std::string& str = "");
};

} // namespace oxide
