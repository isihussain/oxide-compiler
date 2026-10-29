#pragma once

#include "ast.hpp"
#include <sstream>
#include <string>

namespace oxide {

class CCodeGenerator {
public:
    explicit CCodeGenerator(const Program& program, bool aerospace_profile = false);
    std::string generate();

private:
    void emit_includes();
    void emit_aerospace_traps();
    void emit_struct_declarations();
    void emit_forward_declarations();
    void emit_function(const Function& func);
    void emit_block(const BlockExpr& block, bool is_function_body);
    void emit_statement(const Statement& stmt);
    void emit_expression(const Expression& expr);
    std::string map_type(const Type& type);

    const Program& program_;
    bool aerospace_profile_ = false;
    std::stringstream out_;
    int indent_level_ = 0;

    void indent();
    void line(const std::string& str = "");
};

} // namespace oxide
