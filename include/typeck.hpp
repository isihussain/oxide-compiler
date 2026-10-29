#pragma once

#include "ast.hpp"
#include <map>
#include <string>
#include <vector>

namespace oxide {

struct FunctionSignature {
    std::string name;
    std::vector<Type> param_types;
    Type return_type;
    SourceLocation location;
};

class TypeChecker {
public:
    explicit TypeChecker(Program& program);
    bool check();
    const std::vector<std::string>& errors() const { return errors_; }

private:
    void register_functions();
    void check_function(Function& func);
    void check_block(BlockExpr& block, const Type& expected_return_type, bool is_function_root);
    void check_statement(Statement& stmt);
    Type check_expression(Expression& expr);

    void enter_scope();
    void exit_scope();
    void declare_variable(const std::string& name, const Type& type, bool is_mut, SourceLocation loc);
    std::optional<std::pair<Type, bool>> lookup_variable(const std::string& name);

    void emit_error(SourceLocation loc, const std::string& message);

    Program& program_;
    std::map<std::string, FunctionSignature> function_table_;
    std::vector<std::map<std::string, std::pair<Type, bool>>> scopes_; // name -> (Type, is_mut)
    std::vector<std::string> errors_;
};

} // namespace oxide
