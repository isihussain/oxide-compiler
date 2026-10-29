#include "typeck.hpp"
#include <set>
#include <sstream>

namespace oxide {

TypeChecker::TypeChecker(Program& program) : program_(program) {}

void TypeChecker::emit_error(SourceLocation loc, const std::string& message) {
    std::ostringstream ss;
    ss << "error[" << loc.line << ":" << loc.column << "]: " << message;
    errors_.push_back(ss.str());
}

void TypeChecker::enter_scope() {
    scopes_.emplace_back();
}

void TypeChecker::exit_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

void TypeChecker::declare_variable(const std::string& name, const Type& type, bool is_mut, SourceLocation loc) {
    if (scopes_.empty()) return;
    auto& current = scopes_.back();
    if (current.find(name) != current.end()) {
        emit_error(loc, "re-declaration of variable '" + name + "' in same scope");
        return;
    }
    current[name] = {type, is_mut};
}

std::optional<std::pair<Type, bool>> TypeChecker::lookup_variable(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

void TypeChecker::register_structs() {
    for (const auto& s : program_.structs) {
        if (struct_table_.find(s->name) != struct_table_.end()) {
            emit_error(s->location, "duplicate struct definition: '" + s->name + "'");
            continue;
        }
        StructSignature sig;
        sig.name = s->name;
        sig.location = s->location;
        for (const auto& f : s->fields) {
            if (sig.fields.find(f.name) != sig.fields.end()) {
                emit_error(f.location, "duplicate field '" + f.name + "' in struct '" + s->name + "'");
            }
            sig.fields[f.name] = f.type;
            sig.field_order.push_back(f.name);
        }
        struct_table_[s->name] = std::move(sig);
    }
}

void TypeChecker::register_functions() {
    for (const auto& fn : program_.functions) {
        if (function_table_.find(fn->name) != function_table_.end()) {
            emit_error(fn->location, "duplicate function definition: '" + fn->name + "'");
            continue;
        }
        std::vector<Type> params;
        for (const auto& p : fn->parameters) {
            params.push_back(p.type);
        }
        function_table_[fn->name] = FunctionSignature{fn->name, params, fn->return_type, fn->location};
    }
}

bool TypeChecker::check() {
    register_structs();
    register_functions();
    for (auto& fn : program_.functions) {
        check_function(*fn);
    }
    return errors_.empty();
}

void TypeChecker::check_function(Function& func) {
    enter_scope();
    for (const auto& p : func.parameters) {
        declare_variable(p.name, p.type, p.is_mut, p.location);
    }

    check_block(*func.body, func.return_type, true);
    exit_scope();
}

void TypeChecker::check_block(BlockExpr& block, const Type& expected_return_type, bool is_function_root) {
    enter_scope();
    for (auto& stmt : block.statements) {
        check_statement(*stmt);
    }

    Type block_type{BaseTypeKind::Unit, false, false};
    if (block.trailing_expr) {
        block_type = check_expression(*block.trailing_expr);
    }
    block.evaluated_type = block_type;

    if (is_function_root) {
        if (!(block_type == expected_return_type)) {
            emit_error(block.location, "mismatched types: function expected return type '" +
                       expected_return_type.to_string() + "', but block evaluates to '" +
                       block_type.to_string() + "'");
        }
    }

    exit_scope();
}

void TypeChecker::check_statement(Statement& stmt) {
    if (auto* let = dynamic_cast<LetStmt*>(&stmt)) {
        Type deduced_type = let->declared_type;
        if (let->initializer) {
            Type init_type = check_expression(*let->initializer);
            if (let->declared_type.kind != BaseTypeKind::Unit) {
                if (!(let->declared_type == init_type)) {
                    emit_error(let->location, "mismatched types in 'let': expected '" +
                               let->declared_type.to_string() + "', found '" +
                               init_type.to_string() + "'");
                }
            } else {
                deduced_type = init_type;
            }
        }
        declare_variable(let->var_name, deduced_type, let->is_mutable, let->location);
    } else if (auto* expr_stmt = dynamic_cast<ExprStmt*>(&stmt)) {
        if (expr_stmt->expr) {
            check_expression(*expr_stmt->expr);
        }
    }
}

Type TypeChecker::check_expression(Expression& expr) {
    if (auto* int_lit = dynamic_cast<IntegerLiteralExpr*>(&expr)) {
        int_lit->evaluated_type = Type{BaseTypeKind::I32, false, false};
        return int_lit->evaluated_type;
    }

    if (auto* bool_lit = dynamic_cast<BoolLiteralExpr*>(&expr)) {
        bool_lit->evaluated_type = Type{BaseTypeKind::Bool, false, false};
        return bool_lit->evaluated_type;
    }

    if (auto* var_expr = dynamic_cast<VariableExpr*>(&expr)) {
        auto var_info = lookup_variable(var_expr->name);
        if (!var_info) {
            emit_error(var_expr->location, "cannot find value '" + var_expr->name + "' in this scope");
            var_expr->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return var_expr->evaluated_type;
        }
        var_expr->evaluated_type = var_info->first;
        return var_expr->evaluated_type;
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        Type left_t = check_expression(*bin->left);
        Type right_t = check_expression(*bin->right);

        switch (bin->op) {
            case BinaryOp::Add:
            case BinaryOp::Sub:
            case BinaryOp::Mul:
            case BinaryOp::Div: {
                if (left_t.kind != BaseTypeKind::I32 || right_t.kind != BaseTypeKind::I32) {
                    emit_error(bin->location, "arithmetic operations only supported for 'i32'");
                }
                bin->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return bin->evaluated_type;
            }
            case BinaryOp::Equal:
            case BinaryOp::NotEqual: {
                if (!(left_t == right_t)) {
                    emit_error(bin->location, "cannot compare differing types '" +
                               left_t.to_string() + "' and '" + right_t.to_string() + "'");
                }
                bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return bin->evaluated_type;
            }
            case BinaryOp::Less:
            case BinaryOp::LessEqual:
            case BinaryOp::Greater:
            case BinaryOp::GreaterEqual: {
                if (left_t.kind != BaseTypeKind::I32 || right_t.kind != BaseTypeKind::I32) {
                    emit_error(bin->location, "order comparison only supported for 'i32'");
                }
                bin->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return bin->evaluated_type;
            }
        }
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        Type inner_t = check_expression(*un->operand);
        switch (un->op) {
            case UnaryOp::Negate: {
                if (inner_t.kind != BaseTypeKind::I32) {
                    emit_error(un->location, "unary minus only valid on 'i32'");
                }
                un->evaluated_type = Type{BaseTypeKind::I32, false, false};
                return un->evaluated_type;
            }
            case UnaryOp::Not: {
                if (inner_t.kind != BaseTypeKind::Bool) {
                    emit_error(un->location, "unary not '!' only valid on 'bool'");
                }
                un->evaluated_type = Type{BaseTypeKind::Bool, false, false};
                return un->evaluated_type;
            }
            case UnaryOp::Ref: {
                Type ref_t = inner_t;
                ref_t.is_ref = true;
                ref_t.is_mut_ref = false;
                un->evaluated_type = ref_t;
                return un->evaluated_type;
            }
            case UnaryOp::RefMut: {
                Type ref_t = inner_t;
                ref_t.is_ref = true;
                ref_t.is_mut_ref = true;
                un->evaluated_type = ref_t;
                return un->evaluated_type;
            }
            case UnaryOp::Deref: {
                if (!inner_t.is_ref) {
                    emit_error(un->location, "type '" + inner_t.to_string() + "' cannot be dereferenced");
                }
                Type deref_t = inner_t;
                deref_t.is_ref = false;
                deref_t.is_mut_ref = false;
                un->evaluated_type = deref_t;
                return un->evaluated_type;
            }
        }
    }

    if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        auto var_info = lookup_variable(assign->target_var);
        if (!var_info) {
            emit_error(assign->location, "cannot find variable '" + assign->target_var + "' to assign");
        } else {
            Type val_t = check_expression(*assign->value);
            if (!(var_info->first == val_t)) {
                emit_error(assign->location, "mismatched types in assignment to '" + assign->target_var +
                           "': variable is '" + var_info->first.to_string() + "', value is '" +
                           val_t.to_string() + "'");
            }
        }
        assign->evaluated_type = Type{BaseTypeKind::Unit, false, false};
        return assign->evaluated_type;
    }

    if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        auto fn_it = function_table_.find(call->callee);
        if (fn_it == function_table_.end()) {
            emit_error(call->location, "cannot find function '" + call->callee + "' in this scope");
            call->evaluated_type = Type{BaseTypeKind::Unit, false, false};
            return call->evaluated_type;
        }

        const auto& sig = fn_it->second;
        if (call->arguments.size() != sig.param_types.size()) {
            emit_error(call->location, "function '" + call->callee + "' takes " +
                       std::to_string(sig.param_types.size()) + " arguments but " +
                       std::to_string(call->arguments.size()) + " were supplied");
        } else {
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type arg_t = check_expression(*call->arguments[i]);
                if (!(arg_t == sig.param_types[i])) {
                    emit_error(call->arguments[i]->location, "mismatched type in argument " +
                               std::to_string(i + 1) + ": expected '" + sig.param_types[i].to_string() +
                               "', found '" + arg_t.to_string() + "'");
                }
            }
        }
        call->evaluated_type = sig.return_type;
        return call->evaluated_type;
    }

    if (auto* if_expr = dynamic_cast<IfExpr*>(&expr)) {
        Type cond_t = check_expression(*if_expr->condition);
        if (cond_t.kind != BaseTypeKind::Bool) {
            emit_error(if_expr->condition->location, "mismatched types: expected 'bool', found '" +
                       cond_t.to_string() + "' in 'if' condition");
        }

        check_block(*if_expr->then_branch, Type{BaseTypeKind::Unit, false, false}, false);
        Type then_t = if_expr->then_branch->evaluated_type;

        if (if_expr->else_branch) {
            check_block(*if_expr->else_branch, Type{BaseTypeKind::Unit, false, false}, false);
            Type else_t = if_expr->else_branch->evaluated_type;
            if (!(then_t == else_t)) {
                emit_error(if_expr->location, "'if' and 'else' have incompatible types: '" +
                           then_t.to_string() + "' versus '" + else_t.to_string() + "'");
            }
            if_expr->evaluated_type = then_t;
        } else {
            if_expr->evaluated_type = Type{BaseTypeKind::Unit, false, false};
        }
        return if_expr->evaluated_type;
    }

    if (auto* ret = dynamic_cast<ReturnExpr*>(&expr)) {
        if (ret->value) {
            ret->evaluated_type = check_expression(*ret->value);
        } else {
            ret->evaluated_type = Type{BaseTypeKind::Unit, false, false};
        }
        return ret->evaluated_type;
    }

    if (auto* block = dynamic_cast<BlockExpr*>(&expr)) {
        check_block(*block, Type{BaseTypeKind::Unit, "", false, false}, false);
        return block->evaluated_type;
    }

    if (auto* while_expr = dynamic_cast<WhileExpr*>(&expr)) {
        Type cond_t = check_expression(*while_expr->condition);
        if (cond_t.kind != BaseTypeKind::Bool) {
            emit_error(while_expr->condition->location, "mismatched types: expected 'bool', found '" +
                       cond_t.to_string() + "' in 'while' condition");
        }
        check_block(*while_expr->body, Type{BaseTypeKind::Unit, "", false, false}, false);
        while_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
        return while_expr->evaluated_type;
    }

    if (auto* s_init = dynamic_cast<StructInitExpr*>(&expr)) {
        auto it = struct_table_.find(s_init->struct_name);
        if (it == struct_table_.end()) {
            emit_error(s_init->location, "cannot find struct '" + s_init->struct_name + "' in this scope");
            s_init->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return s_init->evaluated_type;
        }
        const auto& sig = it->second;
        std::set<std::string> provided_fields;
        for (auto& f : s_init->fields) {
            if (provided_fields.count(f.field_name)) {
                emit_error(f.location, "field '" + f.field_name + "' specified more than once");
            }
            provided_fields.insert(f.field_name);
            auto field_it = sig.fields.find(f.field_name);
            if (field_it == sig.fields.end()) {
                emit_error(f.location, "struct '" + s_init->struct_name + "' has no field named '" + f.field_name + "'");
            } else {
                Type val_t = check_expression(*f.value);
                if (!(val_t == field_it->second)) {
                    emit_error(f.location, "mismatched type for field '" + f.field_name + "': expected '" +
                               field_it->second.to_string() + "', found '" + val_t.to_string() + "'");
                }
            }
        }
        for (const auto& expected_field : sig.field_order) {
            if (!provided_fields.count(expected_field)) {
                emit_error(s_init->location, "missing field '" + expected_field + "' in initializer of '" + s_init->struct_name + "'");
            }
        }
        s_init->evaluated_type = Type{BaseTypeKind::Named, s_init->struct_name, false, false};
        return s_init->evaluated_type;
    }

    if (auto* fa = dynamic_cast<FieldAccessExpr*>(&expr)) {
        Type target_t = check_expression(*fa->target);
        if (target_t.kind != BaseTypeKind::Named) {
            emit_error(fa->location, "type '" + target_t.to_string() + "' does not have fields");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        auto it = struct_table_.find(target_t.name);
        if (it == struct_table_.end()) {
            emit_error(fa->location, "unknown struct '" + target_t.name + "'");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        auto field_it = it->second.fields.find(fa->field_name);
        if (field_it == it->second.fields.end()) {
            emit_error(fa->location, "no field '" + fa->field_name + "' on type '" + target_t.name + "'");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        fa->evaluated_type = field_it->second;
        return fa->evaluated_type;
    }

    return Type{BaseTypeKind::Unit, "", false, false};
}

} // namespace oxide
