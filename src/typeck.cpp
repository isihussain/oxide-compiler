#include "typeck.hpp"
#include <optional>
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
        sig.type_params = s->type_params;
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

void TypeChecker::register_enums() {
    for (const auto& e : program_.enums) {
        if (enum_table_.find(e->name) != enum_table_.end() || struct_table_.find(e->name) != struct_table_.end()) {
            emit_error(e->location, "duplicate type definition: '" + e->name + "'");
            continue;
        }
        EnumSignature sig;
        sig.name = e->name;
        sig.location = e->location;
        for (const auto& v : e->variants) {
            if (sig.variants.find(v.name) != sig.variants.end()) {
                emit_error(v.location, "duplicate variant '" + v.name + "' in enum '" + e->name + "'");
            }
            sig.variants[v.name] = EnumVariantSignature{v.name, v.payload_type, v.tag, v.location};
        }
        enum_table_[e->name] = std::move(sig);
    }
}

void TypeChecker::register_functions() {
    for (const auto& eb : program_.extern_blocks) {
        for (const auto& decl : eb->declarations) {
            if (function_table_.find(decl.name) != function_table_.end()) {
                emit_error(decl.location, "duplicate function definition: '" + decl.name + "'");
                continue;
            }
            std::vector<Type> params;
            for (const auto& p : decl.parameters) {
                params.push_back(p.type);
            }
            function_table_[decl.name] = FunctionSignature{decl.name, {}, params, decl.return_type, decl.location};
        }
    }

    for (const auto& fn : program_.functions) {
        if (function_table_.find(fn->name) != function_table_.end()) {
            emit_error(fn->location, "duplicate function definition: '" + fn->name + "'");
            continue;
        }
        std::vector<Type> params;
        for (const auto& p : fn->parameters) {
            params.push_back(p.type);
        }
        function_table_[fn->name] = FunctionSignature{fn->name, fn->type_params, params, fn->return_type, fn->location};
    }
}

Type TypeChecker::substitute_type(const Type& type, const std::map<std::string, Type>& subst) {
    Type res = type;
    if (res.kind == BaseTypeKind::GenericParam) {
        auto it = subst.find(res.name);
        if (it != subst.end()) {
            Type replaced = it->second;
            replaced.is_ref = res.is_ref || replaced.is_ref;
            replaced.is_mut_ref = res.is_mut_ref || replaced.is_mut_ref;
            return replaced;
        }
    } else if (res.kind == BaseTypeKind::Named) {
        for (auto& arg : res.type_args) {
            arg = substitute_type(arg, subst);
        }
    }
    return res;
}

bool TypeChecker::unify_type(const Type& pattern, const Type& concrete, std::map<std::string, Type>& subst) {
    if (pattern.kind == BaseTypeKind::GenericParam) {
        auto it = subst.find(pattern.name);
        if (it != subst.end()) {
            return it->second == concrete;
        }
        subst[pattern.name] = concrete;
        return true;
    }
    if (pattern.kind != concrete.kind) {
        return false;
    }
    if (pattern.kind == BaseTypeKind::Named) {
        if (pattern.name != concrete.name) return false;
        if (pattern.type_args.size() != concrete.type_args.size()) return false;
        for (size_t i = 0; i < pattern.type_args.size(); ++i) {
            if (!unify_type(pattern.type_args[i], concrete.type_args[i], subst)) {
                return false;
            }
        }
        return true;
    }
    return pattern.kind == concrete.kind;
}

void TypeChecker::register_traits() {
    for (const auto& t : program_.traits) {
        if (trait_table_.find(t->name) != trait_table_.end()) {
            emit_error(t->location, "duplicate trait definition: '" + t->name + "'");
            continue;
        }
        TraitSignature sig;
        sig.name = t->name;
        sig.location = t->location;
        for (const auto& m : t->methods) {
            if (sig.methods.find(m.name) != sig.methods.end()) {
                emit_error(m.location, "duplicate method '" + m.name + "' in trait '" + t->name + "'");
            }
            sig.methods[m.name] = TraitMethodSignature{m.name, m.parameters, m.return_type, m.location};
        }
        trait_table_[t->name] = std::move(sig);
    }
}

void TypeChecker::register_impl_blocks() {
    for (const auto& ib : program_.impl_blocks) {
        if (struct_table_.find(ib->target_type) == struct_table_.end()) {
            emit_error(ib->location, "cannot find type '" + ib->target_type + "' in this scope");
        }

        if (ib->trait_name.has_value()) {
            const std::string& trait_name = *ib->trait_name;
            auto t_it = trait_table_.find(trait_name);
            if (t_it == trait_table_.end()) {
                emit_error(ib->location, "cannot find trait '" + trait_name + "' in this scope");
                continue;
            }
            const auto& trait_sig = t_it->second;

            // Check trait completeness: ensure all trait methods are implemented
            for (const auto& [req_name, req_method] : trait_sig.methods) {
                bool found = false;
                for (const auto& m : ib->methods) {
                    if (m->method_name == req_name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_error(ib->location, "missing items in 'impl " + trait_name + " for " +
                               ib->target_type + "': method '" + req_name + "' not implemented");
                }
            }

            // Check signatures of implemented trait methods
            for (const auto& m : ib->methods) {
                auto req_it = trait_sig.methods.find(m->method_name);
                if (req_it == trait_sig.methods.end()) {
                    emit_error(m->location, "method '" + m->method_name + "' is not a member of trait '" + trait_name + "'");
                    continue;
                }
                const auto& req_method = req_it->second;

                if (m->parameters.size() != req_method.parameters.size()) {
                    emit_error(m->location, "method '" + m->method_name + "' has " +
                               std::to_string(m->parameters.size()) + " parameters but the trait requires " +
                               std::to_string(req_method.parameters.size()));
                } else {
                    for (size_t i = 0; i < m->parameters.size(); ++i) {
                        Type exp_type = req_method.parameters[i].type;
                        if (exp_type.name == "Self") {
                            exp_type.name = ib->target_type;
                        }
                        if (m->parameters[i].type != exp_type) {
                            emit_error(m->parameters[i].location, "method '" + m->method_name +
                                       "' has an incompatible type for parameter '" +
                                       m->parameters[i].name + "': expected '" + exp_type.to_string() +
                                       "', found '" + m->parameters[i].type.to_string() + "'");
                        }
                    }
                }

                if (m->return_type != req_method.return_type) {
                    emit_error(m->location, "method '" + m->method_name +
                               "' has an incompatible return type: expected '" +
                               req_method.return_type.to_string() + "', found '" +
                               m->return_type.to_string() + "'");
                }
            }
        }

        // Register methods in method_table_ and function_table_
        for (const auto& m : ib->methods) {
            std::pair<std::string, std::string> key{ib->target_type, m->method_name};
            if (method_table_.find(key) != method_table_.end()) {
                emit_error(m->location, "duplicate method '" + m->method_name + "' for type '" + ib->target_type + "'");
            }
            method_table_[key] = MethodSignature{
                ib->target_type, m->method_name, m->name, m->parameters, m->return_type, ib->trait_name, m->location
            };

            std::vector<Type> param_types;
            for (const auto& p : m->parameters) {
                param_types.push_back(p.type);
            }
            function_table_[m->name] = FunctionSignature{m->name, m->type_params, param_types, m->return_type, m->location};
            function_table_[ib->target_type + "::" + m->method_name] = FunctionSignature{
                ib->target_type + "::" + m->method_name, m->type_params, param_types, m->return_type, m->location
            };
        }
    }
}

bool TypeChecker::check() {
    register_traits();
    register_structs();
    register_enums();
    register_functions();
    register_impl_blocks();
    for (auto& fn : program_.functions) {
        check_function(*fn);
    }
    for (auto& ib : program_.impl_blocks) {
        for (auto& fn : ib->methods) {
            check_function(*fn);
        }
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
                let->declared_type = init_type;
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
        Type target_t = check_expression(*assign->target);
        Type val_t = check_expression(*assign->value);
        if (!(target_t == val_t)) {
            emit_error(assign->location, "mismatched types in assignment: target is '" +
                       target_t.to_string() + "', value is '" + val_t.to_string() + "'");
        }
        assign->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
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
            call->evaluated_type = sig.return_type;
            return call->evaluated_type;
        }

        std::map<std::string, Type> subst;
        if (!sig.type_params.empty()) {
            if (!call->type_args.empty()) {
                if (call->type_args.size() != sig.type_params.size()) {
                    emit_error(call->location, "function '" + call->callee + "' takes " +
                               std::to_string(sig.type_params.size()) + " type arguments but " +
                               std::to_string(call->type_args.size()) + " were supplied");
                } else {
                    for (size_t i = 0; i < sig.type_params.size(); ++i) {
                        subst[sig.type_params[i]] = call->type_args[i];
                    }
                }
            }

            // Type check arguments and infer missing type parameters
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type arg_t = check_expression(*call->arguments[i]);
                unify_type(sig.param_types[i], arg_t, subst);
            }

            // Fill in call->type_args if it was inferred
            if (call->type_args.empty()) {
                for (const auto& tp : sig.type_params) {
                    auto it = subst.find(tp);
                    if (it != subst.end()) {
                        call->type_args.push_back(it->second);
                    } else {
                        emit_error(call->location, "cannot infer type argument for type parameter '" + tp +
                                   "' in call to '" + call->callee + "'");
                    }
                }
            }

            // Check argument types against substituted parameter types
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type expected_t = substitute_type(sig.param_types[i], subst);
                Type arg_t = call->arguments[i]->evaluated_type;
                if (!(arg_t == expected_t)) {
                    emit_error(call->arguments[i]->location, "mismatched type in argument " +
                               std::to_string(i + 1) + ": expected '" + expected_t.to_string() +
                               "', found '" + arg_t.to_string() + "'");
                }
            }
            call->evaluated_type = substitute_type(sig.return_type, subst);
            return call->evaluated_type;
        } else {
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                Type arg_t = check_expression(*call->arguments[i]);
                if (!(arg_t == sig.param_types[i])) {
                    emit_error(call->arguments[i]->location, "mismatched type in argument " +
                               std::to_string(i + 1) + ": expected '" + sig.param_types[i].to_string() +
                               "', found '" + arg_t.to_string() + "'");
                }
            }
            call->evaluated_type = sig.return_type;
            return call->evaluated_type;
        }
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
        std::map<std::string, Type> subst;
        if (!sig.type_params.empty()) {
            if (!s_init->type_args.empty()) {
                if (s_init->type_args.size() != sig.type_params.size()) {
                    emit_error(s_init->location, "struct '" + s_init->struct_name + "' takes " +
                               std::to_string(sig.type_params.size()) + " type arguments but " +
                               std::to_string(s_init->type_args.size()) + " were supplied");
                } else {
                    for (size_t i = 0; i < sig.type_params.size(); ++i) {
                        subst[sig.type_params[i]] = s_init->type_args[i];
                    }
                }
            }
        }

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
                if (!sig.type_params.empty()) {
                    unify_type(field_it->second, val_t, subst);
                }
            }
        }

        if (!sig.type_params.empty() && s_init->type_args.empty()) {
            for (const auto& tp : sig.type_params) {
                auto sit = subst.find(tp);
                if (sit != subst.end()) {
                    s_init->type_args.push_back(sit->second);
                } else {
                    emit_error(s_init->location, "cannot infer type argument for type parameter '" + tp +
                               "' in struct initializer of '" + s_init->struct_name + "'");
                }
            }
        }

        // Validate field types against substituted expected types
        for (auto& f : s_init->fields) {
            auto field_it = sig.fields.find(f.field_name);
            if (field_it != sig.fields.end()) {
                Type expected_t = substitute_type(field_it->second, subst);
                Type val_t = f.value->evaluated_type;
                if (!(val_t == expected_t)) {
                    emit_error(f.location, "mismatched type for field '" + f.field_name + "': expected '" +
                               expected_t.to_string() + "', found '" + val_t.to_string() + "'");
                }
            }
        }

        for (const auto& expected_field : sig.field_order) {
            if (!provided_fields.count(expected_field)) {
                emit_error(s_init->location, "missing field '" + expected_field + "' in initializer of '" + s_init->struct_name + "'");
            }
        }
        s_init->evaluated_type = Type{BaseTypeKind::Named, s_init->struct_name, false, false, s_init->type_args};
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
        const auto& sig = it->second;
        auto field_it = sig.fields.find(fa->field_name);
        if (field_it == sig.fields.end()) {
            emit_error(fa->location, "no field '" + fa->field_name + "' on type '" + target_t.name + "'");
            fa->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return fa->evaluated_type;
        }
        std::map<std::string, Type> subst;
        if (!sig.type_params.empty() && target_t.type_args.size() == sig.type_params.size()) {
            for (size_t i = 0; i < sig.type_params.size(); ++i) {
                subst[sig.type_params[i]] = target_t.type_args[i];
            }
        }
        fa->evaluated_type = substitute_type(field_it->second, subst);
        return fa->evaluated_type;
    }

    if (auto* mc = dynamic_cast<MethodCallExpr*>(&expr)) {
        Type receiver_t = check_expression(*mc->receiver);
        std::string base_struct = receiver_t.name;
        if (receiver_t.kind != BaseTypeKind::Named || struct_table_.find(base_struct) == struct_table_.end()) {
            emit_error(mc->location, "cannot call method '" + mc->method_name + "' on non-struct type '" + receiver_t.to_string() + "'");
            mc->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return mc->evaluated_type;
        }

        auto it = method_table_.find({base_struct, mc->method_name});
        if (it == method_table_.end()) {
            emit_error(mc->location, "no method named '" + mc->method_name + "' found for type '" + base_struct + "'");
            mc->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return mc->evaluated_type;
        }

        const auto& method_sig = it->second;
        if (method_sig.parameters.empty()) {
            emit_error(mc->location, "method '" + mc->method_name + "' on type '" + base_struct + "' is an associated function without a self receiver");
            mc->evaluated_type = method_sig.return_type;
            return mc->evaluated_type;
        }

        const auto& self_param = method_sig.parameters[0];
        SelfKind self_kind = SelfKind::Value;
        if (self_param.type.is_ref) {
            self_kind = self_param.type.is_mut_ref ? SelfKind::RefMut : SelfKind::Ref;
        }
        mc->resolved_self_kind = self_kind;
        mc->resolved_callee = method_sig.mangled_name;

        // Check mutability if method takes &mut self
        if (self_kind == SelfKind::RefMut) {
            if (auto* var_expr = dynamic_cast<VariableExpr*>(mc->receiver.get())) {
                auto var_info = lookup_variable(var_expr->name);
                if (var_info && !var_info->second && !receiver_t.is_mut_ref) {
                    emit_error(mc->location, "cannot borrow immutable local variable '" + var_expr->name + "' as mutable in method call");
                }
            }
            if (receiver_t.is_ref && !receiver_t.is_mut_ref) {
                emit_error(mc->location, "cannot borrow data in a '&' reference as mutable");
            }
        }

        size_t expected_args = method_sig.parameters.size() - 1;
        if (mc->arguments.size() != expected_args) {
            emit_error(mc->location, "method '" + mc->method_name + "' takes " +
                       std::to_string(expected_args) + " arguments but " +
                       std::to_string(mc->arguments.size()) + " were supplied");
            mc->evaluated_type = method_sig.return_type;
            return mc->evaluated_type;
        }

        for (size_t i = 0; i < mc->arguments.size(); ++i) {
            Type arg_t = check_expression(*mc->arguments[i]);
            Type exp_t = method_sig.parameters[i + 1].type;
            if (!(arg_t == exp_t)) {
                emit_error(mc->arguments[i]->location, "mismatched type in argument " + std::to_string(i + 1) +
                           " to method '" + mc->method_name + "': expected '" + exp_t.to_string() +
                           "', found '" + arg_t.to_string() + "'");
            }
        }

        mc->evaluated_type = method_sig.return_type;
        return mc->evaluated_type;
    }

    if (auto* ev = dynamic_cast<EnumVariantExpr*>(&expr)) {
        auto it = enum_table_.find(ev->enum_name);
        if (it == enum_table_.end()) {
            emit_error(ev->location, "cannot find enum '" + ev->enum_name + "' in this scope");
            ev->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return ev->evaluated_type;
        }
        const auto& sig = it->second;
        auto var_it = sig.variants.find(ev->variant_name);
        if (var_it == sig.variants.end()) {
            emit_error(ev->location, "enum '" + ev->enum_name + "' has no variant named '" + ev->variant_name + "'");
            ev->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return ev->evaluated_type;
        }
        const auto& var_sig = var_it->second;
        if (var_sig.payload_type.has_value()) {
            if (!ev->payload) {
                emit_error(ev->location, "variant '" + ev->enum_name + "::" + ev->variant_name + "' requires a payload of type '" +
                           var_sig.payload_type->to_string() + "'");
            } else {
                Type payload_t = check_expression(*ev->payload);
                if (!(payload_t == *var_sig.payload_type)) {
                    emit_error(ev->location, "mismatched payload type for variant '" + ev->enum_name + "::" + ev->variant_name +
                               "': expected '" + var_sig.payload_type->to_string() + "', found '" + payload_t.to_string() + "'");
                }
            }
        } else {
            if (ev->payload) {
                emit_error(ev->location, "variant '" + ev->enum_name + "::" + ev->variant_name + "' does not take a payload");
                check_expression(*ev->payload);
            }
        }
        ev->evaluated_type = Type{BaseTypeKind::Named, ev->enum_name, false, false};
        return ev->evaluated_type;
    }

    if (auto* match_expr = dynamic_cast<MatchExpr*>(&expr)) {
        Type target_t = check_expression(*match_expr->target);
        if (target_t.kind != BaseTypeKind::Named) {
            emit_error(match_expr->location, "cannot match on non-enum type '" + target_t.to_string() + "'");
            match_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return match_expr->evaluated_type;
        }
        auto it = enum_table_.find(target_t.name);
        if (it == enum_table_.end()) {
            emit_error(match_expr->location, "cannot match on unknown enum '" + target_t.name + "'");
            match_expr->evaluated_type = Type{BaseTypeKind::Unit, "", false, false};
            return match_expr->evaluated_type;
        }
        const auto& enum_sig = it->second;
        std::set<std::string> covered_variants;
        bool has_wildcard = false;
        std::optional<Type> expected_arm_type;

        for (auto& arm : match_expr->arms) {
            enter_scope();
            if (arm.pattern.kind == PatternKind::Wildcard) {
                has_wildcard = true;
            } else {
                if (arm.pattern.enum_name != enum_sig.name) {
                    emit_error(arm.pattern.location, "mismatched enum in pattern: expected '" + enum_sig.name +
                               "', found '" + arm.pattern.enum_name + "'");
                }
                auto var_it = enum_sig.variants.find(arm.pattern.variant_name);
                if (var_it == enum_sig.variants.end()) {
                    emit_error(arm.pattern.location, "enum '" + enum_sig.name + "' has no variant named '" +
                               arm.pattern.variant_name + "'");
                } else {
                    const auto& var_sig = var_it->second;
                    if (var_sig.payload_type.has_value()) {
                        if (arm.pattern.binding_name.empty()) {
                            emit_error(arm.pattern.location, "pattern for variant '" + arm.pattern.variant_name +
                                       "' must bind payload variable");
                        } else {
                            declare_variable(arm.pattern.binding_name, *var_sig.payload_type, false, arm.pattern.location);
                        }
                    } else {
                        if (!arm.pattern.binding_name.empty()) {
                            emit_error(arm.pattern.location, "variant '" + arm.pattern.variant_name +
                                       "' has no payload to bind");
                        }
                    }
                    covered_variants.insert(arm.pattern.variant_name);
                }
            }

            Type arm_t = check_expression(*arm.body);
            exit_scope();

            if (!expected_arm_type.has_value()) {
                expected_arm_type = arm_t;
            } else if (!(*expected_arm_type == arm_t)) {
                emit_error(arm.location, "match arms have incompatible types: expected '" +
                           expected_arm_type->to_string() + "', found '" + arm_t.to_string() + "'");
            }
        }

        // Exhaustiveness check
        if (!has_wildcard) {
            for (const auto& [vname, _] : enum_sig.variants) {
                if (covered_variants.find(vname) == covered_variants.end()) {
                    emit_error(match_expr->location, "non-exhaustive patterns: variant '" + vname + "' not covered");
                }
            }
        }

        match_expr->evaluated_type = expected_arm_type.value_or(Type{BaseTypeKind::Unit, "", false, false});
        return match_expr->evaluated_type;
    }

    return Type{BaseTypeKind::Unit, "", false, false};
}

} // namespace oxide
