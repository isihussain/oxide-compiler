#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace oxide {

struct VarState {
    std::string name;
    bool is_mutable = false;
    bool is_initialized = false;
    int immutable_borrows = 0;
    bool has_mutable_borrow = false;
    SourceLocation declared_at;
};

class BorrowChecker {
public:
    explicit BorrowChecker(Program& program);
    bool check();
    const std::vector<std::string>& errors() const { return errors_; }

private:
    void check_function(Function& func);
    void check_block(BlockExpr& block);
    void check_statement(Statement& stmt);
    void check_expression(Expression& expr);

    void enter_scope();
    void exit_scope();
    void declare_variable(const std::string& name, bool is_mut, bool initialized, SourceLocation loc);
    VarState* lookup_variable(const std::string& name);

    void emit_error(SourceLocation loc, const std::string& message);

    Program& program_;
    std::vector<std::map<std::string, VarState>> scopes_;
    std::vector<std::string> errors_;
};

} // namespace oxide
