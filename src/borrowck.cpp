#include "borrowck.hpp"
#include <sstream>

namespace oxide {

BorrowChecker::BorrowChecker(Program& program) : program_(program) {}

void BorrowChecker::emit_error(SourceLocation loc, const std::string& message) {
    std::ostringstream ss;
    ss << "borrowck error[" << loc.line << ":" << loc.column << "]: " << message;
    errors_.push_back(ss.str());
}

void BorrowChecker::enter_scope() {
    scopes_.emplace_back();
}

void BorrowChecker::exit_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void BorrowChecker::declare_variable(const std::string& name, bool is_mut, bool initialized, SourceLocation loc) {
    if (scopes_.empty()) return;
    VarState state;
    state.name = name;
    state.is_mutable = is_mut;
    state.is_initialized = initialized;
    state.declared_at = loc;
    scopes_.back()[name] = state;
}

VarState* BorrowChecker::lookup_variable(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

bool BorrowChecker::check() {
    for (auto& fn : program_.functions) {
        check_function(*fn);
    }
    return errors_.empty();
}

void BorrowChecker::check_function(Function& func) {
    enter_scope();
    for (const auto& p : func.parameters) {
        declare_variable(p.name, p.is_mut, true, p.location);
    }

    check_block(*func.body);
    exit_scope();
}

void BorrowChecker::check_block(BlockExpr& block) {
    enter_scope();
    for (auto& stmt : block.statements) {
        check_statement(*stmt);
    }
    if (block.trailing_expr) {
        check_expression(*block.trailing_expr);
    }
    exit_scope();
}

void BorrowChecker::check_statement(Statement& stmt) {
    if (auto* let = dynamic_cast<LetStmt*>(&stmt)) {
        bool has_init = (let->initializer != nullptr);
        if (has_init) {
            check_expression(*let->initializer);
        }
        declare_variable(let->var_name, let->is_mutable, has_init, let->location);
    } else if (auto* expr_stmt = dynamic_cast<ExprStmt*>(&stmt)) {
        if (expr_stmt->expr) {
            check_expression(*expr_stmt->expr);
        }
    }
}

void BorrowChecker::check_expression(Expression& expr) {
    if (auto* var = dynamic_cast<VariableExpr*>(&expr)) {
        VarState* st = lookup_variable(var->name);
        if (st) {
            if (!st->is_initialized) {
                emit_error(var->location, "use of possibly-uninitialized variable: '" + var->name + "'");
            }
        }
        return;
    }

    if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        VarState* st = lookup_variable(assign->target_var);
        if (st) {
            if (!st->is_mutable && st->is_initialized) {
                emit_error(assign->location, "cannot assign twice to immutable variable '" + assign->target_var + "'");
            }
            if (st->immutable_borrows > 0 || st->has_mutable_borrow) {
                emit_error(assign->location, "cannot assign to '" + assign->target_var + "' because it is borrowed");
            }
            st->is_initialized = true;
        }
        check_expression(*assign->value);
        return;
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        if (un->op == UnaryOp::RefMut) {
            if (auto* target_var = dynamic_cast<VariableExpr*>(un->operand.get())) {
                VarState* st = lookup_variable(target_var->name);
                if (st) {
                    if (!st->is_mutable) {
                        emit_error(un->location, "cannot borrow immutable local variable '" +
                                   target_var->name + "' as mutable");
                    }
                    if (st->immutable_borrows > 0) {
                        emit_error(un->location, "cannot borrow '" + target_var->name +
                                   "' as mutable because it is also borrowed as immutable");
                    }
                    if (st->has_mutable_borrow) {
                        emit_error(un->location, "cannot borrow '" + target_var->name +
                                   "' as mutable more than once at a time");
                    }
                    st->has_mutable_borrow = true;
                }
            }
        } else if (un->op == UnaryOp::Ref) {
            if (auto* target_var = dynamic_cast<VariableExpr*>(un->operand.get())) {
                VarState* st = lookup_variable(target_var->name);
                if (st) {
                    if (st->has_mutable_borrow) {
                        emit_error(un->location, "cannot borrow '" + target_var->name +
                                   "' as immutable because it is also borrowed as mutable");
                    }
                    st->immutable_borrows++;
                }
            }
        }
        check_expression(*un->operand);
        return;
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        check_expression(*bin->left);
        check_expression(*bin->right);
        return;
    }

    if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        for (auto& arg : call->arguments) {
            check_expression(*arg);
        }
        return;
    }

    if (auto* if_expr = dynamic_cast<IfExpr*>(&expr)) {
        check_expression(*if_expr->condition);
        check_block(*if_expr->then_branch);
        if (if_expr->else_branch) {
            check_block(*if_expr->else_branch);
        }
        return;
    }

    if (auto* ret = dynamic_cast<ReturnExpr*>(&expr)) {
        if (ret->value) {
            check_expression(*ret->value);
        }
        return;
    }

    if (auto* block = dynamic_cast<BlockExpr*>(&expr)) {
        check_block(*block);
        return;
    }

    if (auto* s_init = dynamic_cast<StructInitExpr*>(&expr)) {
        for (auto& f : s_init->fields) {
            check_expression(*f.value);
        }
        return;
    }

    if (auto* fa = dynamic_cast<FieldAccessExpr*>(&expr)) {
        check_expression(*fa->target);
        return;
    }
}

} // namespace oxide
