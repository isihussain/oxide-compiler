#include "monomorphize.hpp"
#include <algorithm>

namespace oxide {

Monomorphizer::Monomorphizer(Program& program) : program_(program) {
    for (const auto& fn : program_.functions) {
        if (fn->is_generic()) {
            generic_functions_[fn->name] = fn.get();
        }
    }
    for (const auto& s : program_.structs) {
        if (s->is_generic()) {
            generic_structs_[s->name] = s.get();
        }
    }
}

Type Monomorphizer::substitute_type(const Type& type, const std::map<std::string, Type>& subst) {
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

std::unique_ptr<Expression> Monomorphizer::clone_expr(const Expression& expr, const std::map<std::string, Type>& subst) {
    if (auto* int_lit = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
        auto res = std::make_unique<IntegerLiteralExpr>(int_lit->value, int_lit->location);
        res->evaluated_type = substitute_type(int_lit->evaluated_type, subst);
        return res;
    }

    if (auto* bool_lit = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        auto res = std::make_unique<BoolLiteralExpr>(bool_lit->value, bool_lit->location);
        res->evaluated_type = substitute_type(bool_lit->evaluated_type, subst);
        return res;
    }

    if (auto* var = dynamic_cast<const VariableExpr*>(&expr)) {
        auto res = std::make_unique<VariableExpr>(var->name, var->location);
        res->evaluated_type = substitute_type(var->evaluated_type, subst);
        return res;
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        auto res = std::make_unique<BinaryExpr>(bin->op, clone_expr(*bin->left, subst),
                                                clone_expr(*bin->right, subst), bin->location);
        res->evaluated_type = substitute_type(bin->evaluated_type, subst);
        return res;
    }

    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        auto res = std::make_unique<UnaryExpr>(un->op, clone_expr(*un->operand, subst), un->location);
        res->evaluated_type = substitute_type(un->evaluated_type, subst);
        return res;
    }

    if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        auto res = std::make_unique<AssignExpr>(clone_expr(*assign->target, subst),
                                                clone_expr(*assign->value, subst), assign->location);
        res->evaluated_type = substitute_type(assign->evaluated_type, subst);
        return res;
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        std::vector<std::unique_ptr<Expression>> args;
        for (const auto& a : call->arguments) {
            args.push_back(clone_expr(*a, subst));
        }
        std::vector<Type> t_args;
        for (const auto& ta : call->type_args) {
            t_args.push_back(substitute_type(ta, subst));
        }
        auto res = std::make_unique<CallExpr>(call->callee, std::move(args), call->location, std::move(t_args));
        res->evaluated_type = substitute_type(call->evaluated_type, subst);
        return res;
    }

    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        auto cond = clone_expr(*if_expr->condition, subst);
        auto then_b = clone_block(*if_expr->then_branch, subst);
        std::unique_ptr<BlockExpr> else_b = if_expr->else_branch ? clone_block(*if_expr->else_branch, subst) : nullptr;
        auto res = std::make_unique<IfExpr>(std::move(cond), std::move(then_b), std::move(else_b), if_expr->location);
        res->evaluated_type = substitute_type(if_expr->evaluated_type, subst);
        return res;
    }

    if (auto* while_expr = dynamic_cast<const WhileExpr*>(&expr)) {
        auto cond = clone_expr(*while_expr->condition, subst);
        auto body = clone_block(*while_expr->body, subst);
        auto res = std::make_unique<WhileExpr>(std::move(cond), std::move(body), while_expr->location);
        res->evaluated_type = substitute_type(while_expr->evaluated_type, subst);
        return res;
    }

    if (auto* ret = dynamic_cast<const ReturnExpr*>(&expr)) {
        auto val = ret->value ? clone_expr(*ret->value, subst) : nullptr;
        auto res = std::make_unique<ReturnExpr>(std::move(val), ret->location);
        res->evaluated_type = substitute_type(ret->evaluated_type, subst);
        return res;
    }

    if (auto* s_init = dynamic_cast<const StructInitExpr*>(&expr)) {
        std::vector<StructFieldInit> fields;
        for (const auto& f : s_init->fields) {
            fields.push_back(StructFieldInit{f.field_name, clone_expr(*f.value, subst), f.location});
        }
        std::vector<Type> t_args;
        for (const auto& ta : s_init->type_args) {
            t_args.push_back(substitute_type(ta, subst));
        }
        auto res = std::make_unique<StructInitExpr>(s_init->struct_name, std::move(fields), s_init->location, std::move(t_args));
        res->evaluated_type = substitute_type(s_init->evaluated_type, subst);
        return res;
    }

    if (auto* fa = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        auto res = std::make_unique<FieldAccessExpr>(clone_expr(*fa->target, subst), fa->field_name, fa->location);
        res->evaluated_type = substitute_type(fa->evaluated_type, subst);
        return res;
    }

    if (auto* ev = dynamic_cast<const EnumVariantExpr*>(&expr)) {
        auto payload = ev->payload ? clone_expr(*ev->payload, subst) : nullptr;
        auto res = std::make_unique<EnumVariantExpr>(ev->enum_name, ev->variant_name, std::move(payload), ev->location);
        res->evaluated_type = substitute_type(ev->evaluated_type, subst);
        return res;
    }

    if (auto* match_expr = dynamic_cast<const MatchExpr*>(&expr)) {
        auto tgt = clone_expr(*match_expr->target, subst);
        std::vector<MatchArm> arms;
        for (const auto& a : match_expr->arms) {
            arms.push_back(MatchArm{a.pattern, clone_expr(*a.body, subst), a.location});
        }
        auto res = std::make_unique<MatchExpr>(std::move(tgt), std::move(arms), match_expr->location);
        res->evaluated_type = substitute_type(match_expr->evaluated_type, subst);
        return res;
    }

    if (auto* block = dynamic_cast<const BlockExpr*>(&expr)) {
        return clone_block(*block, subst);
    }

    return nullptr;
}

std::unique_ptr<Statement> Monomorphizer::clone_stmt(const Statement& stmt, const std::map<std::string, Type>& subst) {
    if (auto* let = dynamic_cast<const LetStmt*>(&stmt)) {
        auto init = let->initializer ? clone_expr(*let->initializer, subst) : nullptr;
        Type dec_type = substitute_type(let->declared_type, subst);
        return std::make_unique<LetStmt>(let->var_name, let->is_mutable, dec_type, std::move(init), let->location);
    }
    if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        auto e = expr_stmt->expr ? clone_expr(*expr_stmt->expr, subst) : nullptr;
        return std::make_unique<ExprStmt>(std::move(e), expr_stmt->location);
    }
    return nullptr;
}

std::unique_ptr<BlockExpr> Monomorphizer::clone_block(const BlockExpr& block, const std::map<std::string, Type>& subst) {
    auto res = std::make_unique<BlockExpr>(block.location);
    for (const auto& s : block.statements) {
        res->statements.push_back(clone_stmt(*s, subst));
    }
    if (block.trailing_expr) {
        res->trailing_expr = clone_expr(*block.trailing_expr, subst);
    }
    res->evaluated_type = substitute_type(block.evaluated_type, subst);
    return res;
}

std::string Monomorphizer::specialize_function(const std::string& orig_name, const std::vector<Type>& type_args) {
    std::string mangled = orig_name;
    for (const auto& arg : type_args) {
        mangled += "__" + arg.mangled_name();
    }
    if (specialized_functions_.find(mangled) != specialized_functions_.end()) {
        return mangled;
    }

    auto it = generic_functions_.find(orig_name);
    if (it == generic_functions_.end()) {
        return orig_name;
    }
    const Function* template_fn = it->second;

    std::map<std::string, Type> subst;
    for (size_t i = 0; i < template_fn->type_params.size() && i < type_args.size(); ++i) {
        subst[template_fn->type_params[i]] = type_args[i];
    }

    std::vector<Parameter> new_params;
    for (const auto& p : template_fn->parameters) {
        Type p_t = substitute_type(p.type, subst);
        transform_type(p_t);
        new_params.push_back(Parameter{p.name, p_t, p.is_mut, p.location});
    }
    Type new_ret = substitute_type(template_fn->return_type, subst);
    transform_type(new_ret);

    auto new_body = clone_block(*template_fn->body, subst);
    auto specialized = std::make_unique<Function>(mangled, std::move(new_params), new_ret,
                                                  std::move(new_body), template_fn->location,
                                                  template_fn->abi, std::vector<std::string>{});
    specialized_functions_[mangled] = std::move(specialized);
    transform_block(*specialized_functions_[mangled]->body);

    return mangled;
}

std::string Monomorphizer::specialize_struct(const std::string& orig_name, const std::vector<Type>& type_args) {
    std::string mangled = orig_name;
    for (const auto& arg : type_args) {
        mangled += "__" + arg.mangled_name();
    }
    if (specialized_structs_.find(mangled) != specialized_structs_.end()) {
        return mangled;
    }

    auto it = generic_structs_.find(orig_name);
    if (it == generic_structs_.end()) {
        return orig_name;
    }
    const StructDef* template_s = it->second;

    std::map<std::string, Type> subst;
    for (size_t i = 0; i < template_s->type_params.size() && i < type_args.size(); ++i) {
        subst[template_s->type_params[i]] = type_args[i];
    }

    std::vector<StructField> new_fields;
    for (const auto& f : template_s->fields) {
        Type f_t = substitute_type(f.type, subst);
        transform_type(f_t);
        new_fields.push_back(StructField{f.name, f_t, f.location});
    }

    auto specialized = std::make_unique<StructDef>(mangled, std::move(new_fields), template_s->location, std::vector<std::string>{});
    specialized_structs_[mangled] = std::move(specialized);

    return mangled;
}

void Monomorphizer::transform_type(Type& type) {
    if (type.kind == BaseTypeKind::Named && !type.type_args.empty()) {
        for (auto& ta : type.type_args) {
            transform_type(ta);
        }
        std::string mangled = specialize_struct(type.name, type.type_args);
        type.name = mangled;
        type.type_args.clear();
    }
}

void Monomorphizer::transform_expr(Expression& expr) {
    transform_type(expr.evaluated_type);

    if (auto* call = dynamic_cast<CallExpr*>(&expr)) {
        for (auto& arg : call->arguments) {
            transform_expr(*arg);
        }
        if (!call->type_args.empty()) {
            for (auto& ta : call->type_args) {
                transform_type(ta);
            }
            call->callee = specialize_function(call->callee, call->type_args);
            call->type_args.clear();
        }
        return;
    }

    if (auto* s_init = dynamic_cast<StructInitExpr*>(&expr)) {
        for (auto& f : s_init->fields) {
            transform_expr(*f.value);
        }
        if (!s_init->type_args.empty()) {
            for (auto& ta : s_init->type_args) {
                transform_type(ta);
            }
            s_init->struct_name = specialize_struct(s_init->struct_name, s_init->type_args);
            s_init->type_args.clear();
        }
        return;
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(&expr)) {
        transform_expr(*bin->left);
        transform_expr(*bin->right);
        return;
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(&expr)) {
        transform_expr(*un->operand);
        return;
    }

    if (auto* assign = dynamic_cast<AssignExpr*>(&expr)) {
        transform_expr(*assign->target);
        transform_expr(*assign->value);
        return;
    }

    if (auto* if_expr = dynamic_cast<IfExpr*>(&expr)) {
        transform_expr(*if_expr->condition);
        transform_block(*if_expr->then_branch);
        if (if_expr->else_branch) {
            transform_block(*if_expr->else_branch);
        }
        return;
    }

    if (auto* while_expr = dynamic_cast<WhileExpr*>(&expr)) {
        transform_expr(*while_expr->condition);
        transform_block(*while_expr->body);
        return;
    }

    if (auto* ret = dynamic_cast<ReturnExpr*>(&expr)) {
        if (ret->value) {
            transform_expr(*ret->value);
        }
        return;
    }

    if (auto* fa = dynamic_cast<FieldAccessExpr*>(&expr)) {
        transform_expr(*fa->target);
        return;
    }

    if (auto* ev = dynamic_cast<EnumVariantExpr*>(&expr)) {
        if (ev->payload) {
            transform_expr(*ev->payload);
        }
        return;
    }

    if (auto* match_expr = dynamic_cast<MatchExpr*>(&expr)) {
        transform_expr(*match_expr->target);
        for (auto& a : match_expr->arms) {
            transform_expr(*a.body);
        }
        return;
    }

    if (auto* block = dynamic_cast<BlockExpr*>(&expr)) {
        transform_block(*block);
        return;
    }
}

void Monomorphizer::transform_stmt(Statement& stmt) {
    if (auto* let = dynamic_cast<LetStmt*>(&stmt)) {
        transform_type(let->declared_type);
        if (let->initializer) {
            transform_expr(*let->initializer);
        }
    } else if (auto* expr_stmt = dynamic_cast<ExprStmt*>(&stmt)) {
        if (expr_stmt->expr) {
            transform_expr(*expr_stmt->expr);
        }
    }
}

void Monomorphizer::transform_block(BlockExpr& block) {
    transform_type(block.evaluated_type);
    for (auto& s : block.statements) {
        transform_stmt(*s);
    }
    if (block.trailing_expr) {
        transform_expr(*block.trailing_expr);
    }
}

void Monomorphizer::run() {
    // 1. Transform non-generic functions
    for (auto& fn : program_.functions) {
        if (!fn->is_generic()) {
            transform_type(fn->return_type);
            for (auto& p : fn->parameters) {
                transform_type(p.type);
            }
            if (fn->body) {
                transform_block(*fn->body);
            }
        }
    }

    // 2. Erase generic function templates
    std::vector<std::unique_ptr<Function>> final_functions;
    for (auto& fn : program_.functions) {
        if (!fn->is_generic()) {
            final_functions.push_back(std::move(fn));
        }
    }
    for (auto& [_, sfn] : specialized_functions_) {
        final_functions.push_back(std::move(sfn));
    }
    program_.functions = std::move(final_functions);

    // 3. Erase generic struct templates
    std::vector<std::unique_ptr<StructDef>> final_structs;
    for (auto& s : program_.structs) {
        if (!s->is_generic()) {
            final_structs.push_back(std::move(s));
        }
    }
    for (auto& [_, ss] : specialized_structs_) {
        final_structs.push_back(std::move(ss));
    }
    program_.structs = std::move(final_structs);
}

} // namespace oxide
