#include "optimizer.hpp"

namespace oxide {

Optimizer::Optimizer(Program& program)
    : program_(program) {}

void Optimizer::collect_inline_candidates() {
    inline_candidates_.clear();
    for (const auto& fn : program_.functions) {
        if (is_inlinable_function(*fn)) {
            inline_candidates_[fn->name] = fn.get();
        }
    }
}

const Expression* Optimizer::get_return_expression(const Function& fn) {
    if (!fn.body) return nullptr;

    if (fn.body->statements.empty() && fn.body->trailing_expr) {
        return fn.body->trailing_expr.get();
    }

    if (fn.body->statements.size() == 1 && !fn.body->trailing_expr) {
        if (auto* es = dynamic_cast<const ExprStmt*>(fn.body->statements[0].get())) {
            if (auto* ret = dynamic_cast<const ReturnExpr*>(es->expr.get())) {
                if (ret->value) {
                    return ret->value.get();
                }
            }
        }
    }

    return nullptr;
}

bool Optimizer::is_pure_argument(const Expression& expr) {
    if (dynamic_cast<const IntegerLiteralExpr*>(&expr)) return true;
    if (dynamic_cast<const FloatLiteralExpr*>(&expr)) return true;
    if (dynamic_cast<const BoolLiteralExpr*>(&expr)) return true;
    if (dynamic_cast<const StringLiteralExpr*>(&expr)) return true;
    if (dynamic_cast<const VariableExpr*>(&expr)) return true;
    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        return is_pure_argument(*un->operand);
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        return is_pure_argument(*bin->left) && is_pure_argument(*bin->right);
    }
    return false;
}

bool Optimizer::is_leaf_expression(const Expression& expr, const std::string& self_name) {
    if (dynamic_cast<const WhileExpr*>(&expr)) return false;
    if (dynamic_cast<const ForExpr*>(&expr)) return false;
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        if (call->callee == self_name) return false;
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        return is_leaf_expression(*bin->left, self_name) && is_leaf_expression(*bin->right, self_name);
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        return is_leaf_expression(*un->operand, self_name);
    }
    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        if (!is_leaf_expression(*if_expr->condition, self_name)) return false;
        if (if_expr->then_branch && if_expr->then_branch->trailing_expr) {
            if (!is_leaf_expression(*if_expr->then_branch->trailing_expr, self_name)) return false;
        }
        if (if_expr->else_branch && if_expr->else_branch->trailing_expr) {
            if (!is_leaf_expression(*if_expr->else_branch->trailing_expr, self_name)) return false;
        }
    }
    return true;
}

int Optimizer::count_ast_nodes(const Expression& expr) {
    int count = 1;
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (bin->left) count += count_ast_nodes(*bin->left);
        if (bin->right) count += count_ast_nodes(*bin->right);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        if (un->operand) count += count_ast_nodes(*un->operand);
    } else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        for (const auto& a : call->arguments) {
            if (a) count += count_ast_nodes(*a);
        }
    } else if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        if (if_expr->condition) count += count_ast_nodes(*if_expr->condition);
        if (if_expr->then_branch) {
            count += static_cast<int>(if_expr->then_branch->statements.size());
            if (if_expr->then_branch->trailing_expr) count += count_ast_nodes(*if_expr->then_branch->trailing_expr);
        }
        if (if_expr->else_branch) {
            count += static_cast<int>(if_expr->else_branch->statements.size());
            if (if_expr->else_branch->trailing_expr) count += count_ast_nodes(*if_expr->else_branch->trailing_expr);
        }
    }
    return count;
}

bool Optimizer::is_inlinable_function(const Function& fn) {
    if (fn.name == "main") return false;
    if (!fn.body) return false;
    if (!fn.abi.empty()) return false;
    if (!fn.type_params.empty()) return false;

    const Expression* ret = get_return_expression(fn);
    if (!ret) return false;

    if (fn.has_attribute("inline") || fn.has_attribute("inline(always)")) {
        return true;
    }

    int nodes = count_ast_nodes(*ret);
    if (nodes <= 20 && is_leaf_expression(*ret, fn.name)) {
        return true;
    }

    return false;
}

void Optimizer::run() {
    collect_inline_candidates();

    for (int iter = 0; iter < 4; ++iter) {
        changed_ = false;
        for (auto& fn : program_.functions) {
            optimize_function(*fn);
        }
        for (auto& ib : program_.impl_blocks) {
            for (auto& fn : ib->methods) {
                optimize_function(*fn);
            }
        }
        if (!changed_) break;
    }
}

void Optimizer::optimize_function(Function& func) {
    if (func.body) {
        optimize_block(*func.body);
    }
}

// Dead code elimination on block statements, loop unrolling, and call inlining.
void Optimizer::optimize_block(BlockExpr& block) {
    if (block.trailing_expr) {
        if (auto* for_expr = dynamic_cast<ForExpr*>(block.trailing_expr.get())) {
            block.statements.push_back(std::make_unique<ExprStmt>(std::move(block.trailing_expr), for_expr->location));
            block.trailing_expr = nullptr;
            changed_ = true;
        }
    }

    std::vector<std::unique_ptr<Statement>> new_stmts;
    bool returned = false;

    for (auto& s : block.statements) {
        if (returned) {
            // Dead code elimination: skip statements following an unconditional return
            changed_ = true;
            continue;
        }

        if (auto* es = dynamic_cast<ExprStmt*>(s.get())) {
            if (auto* for_expr = dynamic_cast<ForExpr*>(es->expr.get())) {
                if (try_unroll_for_loop(*for_expr, new_stmts)) {
                    changed_ = true;
                    continue;
                }
            }
        }

        optimize_statement(s);

        // Check if statement is an unconditional return
        if (auto* es = dynamic_cast<ExprStmt*>(s.get())) {
            if (dynamic_cast<ReturnExpr*>(es->expr.get())) {
                returned = true;
            }
        }
        new_stmts.push_back(std::move(s));
    }

    block.statements = std::move(new_stmts);

    if (returned) {
        if (block.trailing_expr) {
            block.trailing_expr = nullptr;
            changed_ = true;
        }
    } else if (block.trailing_expr) {
        block.trailing_expr = fold_expression(std::move(block.trailing_expr));
    }
}

bool Optimizer::try_unroll_for_loop(ForExpr& for_expr, std::vector<std::unique_ptr<Statement>>& out) {
    if (!for_expr.iterable) return false;

    for_expr.iterable = fold_expression(std::move(for_expr.iterable));

    auto* range = dynamic_cast<RangeExpr*>(for_expr.iterable.get());
    if (!range) return false;

    range->start = fold_expression(std::move(range->start));
    range->end = fold_expression(std::move(range->end));

    auto* s_int = dynamic_cast<IntegerLiteralExpr*>(range->start.get());
    auto* e_int = dynamic_cast<IntegerLiteralExpr*>(range->end.get());
    if (!s_int || !e_int) return false;

    int64_t start_val = s_int->value;
    int64_t end_val = e_int->value;

    if (start_val >= end_val) {
        // Dead loop: 0 iterations executed
        return true;
    }

    int64_t trip_count = end_val - start_val;
    if (trip_count > 8) {
        return false;
    }

    for (int64_t k = start_val; k < end_val; ++k) {
        auto iter_block = std::make_unique<BlockExpr>(for_expr.location);
        iter_block->evaluated_type = Type{BaseTypeKind::Unit};

        auto var_let = std::make_unique<LetStmt>(
            for_expr.var_name,
            false,
            Type{BaseTypeKind::I32},
            std::make_unique<IntegerLiteralExpr>(k, for_expr.location),
            for_expr.location
        );
        iter_block->statements.push_back(std::move(var_let));

        auto lit_k = std::make_unique<IntegerLiteralExpr>(k, for_expr.location);
        lit_k->evaluated_type = Type{BaseTypeKind::I32};
        std::map<std::string, const Expression*> var_subst;
        var_subst[for_expr.var_name] = lit_k.get();

        if (for_expr.body) {
            for (const auto& body_stmt : for_expr.body->statements) {
                if (body_stmt) {
                    iter_block->statements.push_back(clone_stmt(*body_stmt, var_subst));
                }
            }
            if (for_expr.body->trailing_expr) {
                auto trailing_cloned = clone_expr(*for_expr.body->trailing_expr, var_subst);
                iter_block->statements.push_back(std::make_unique<ExprStmt>(std::move(trailing_cloned), for_expr.location));
            }
        }

        optimize_block(*iter_block);
        out.push_back(std::make_unique<ExprStmt>(std::move(iter_block), for_expr.location));
    }

    return true;
}

std::unique_ptr<Expression> Optimizer::try_inline_call(const CallExpr& call) {
    auto it = inline_candidates_.find(call.callee);
    if (it == inline_candidates_.end()) return nullptr;

    const Function* fn = it->second;
    if (!fn || !fn->body) return nullptr;
    if (call.arguments.size() != fn->parameters.size()) return nullptr;

    for (const auto& arg : call.arguments) {
        if (!arg || !is_pure_argument(*arg)) return nullptr;
    }

    const Expression* ret_expr = get_return_expression(*fn);
    if (!ret_expr) return nullptr;

    std::map<std::string, const Expression*> var_subst;
    for (size_t i = 0; i < fn->parameters.size(); ++i) {
        var_subst[fn->parameters[i].name] = call.arguments[i].get();
    }

    auto cloned = clone_expr(*ret_expr, var_subst);
    if (!cloned) return nullptr;

    if (cloned->evaluated_type.kind == BaseTypeKind::Unit && call.evaluated_type.kind != BaseTypeKind::Unit) {
        cloned->evaluated_type = call.evaluated_type;
    }

    return cloned;
}

std::unique_ptr<Expression> Optimizer::clone_expr(
    const Expression& expr,
    const std::map<std::string, const Expression*>& var_subst
) {
    if (auto* int_lit = dynamic_cast<const IntegerLiteralExpr*>(&expr)) {
        auto res = std::make_unique<IntegerLiteralExpr>(int_lit->value, int_lit->location);
        res->evaluated_type = int_lit->evaluated_type;
        return res;
    }

    if (auto* bool_lit = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        auto res = std::make_unique<BoolLiteralExpr>(bool_lit->value, bool_lit->location);
        res->evaluated_type = bool_lit->evaluated_type;
        return res;
    }

    if (auto* fl_lit = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
        auto res = std::make_unique<FloatLiteralExpr>(fl_lit->value, fl_lit->location);
        res->evaluated_type = fl_lit->evaluated_type;
        return res;
    }

    if (auto* str_lit = dynamic_cast<const StringLiteralExpr*>(&expr)) {
        auto res = std::make_unique<StringLiteralExpr>(str_lit->value, str_lit->location);
        res->evaluated_type = str_lit->evaluated_type;
        return res;
    }

    if (auto* var = dynamic_cast<const VariableExpr*>(&expr)) {
        auto it = var_subst.find(var->name);
        if (it != var_subst.end() && it->second) {
            return clone_expr(*it->second, {});
        }
        auto res = std::make_unique<VariableExpr>(var->name, var->location);
        res->evaluated_type = var->evaluated_type;
        return res;
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        auto res = std::make_unique<BinaryExpr>(
            bin->op,
            clone_expr(*bin->left, var_subst),
            clone_expr(*bin->right, var_subst),
            bin->location
        );
        res->evaluated_type = bin->evaluated_type;
        return res;
    }

    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        auto res = std::make_unique<UnaryExpr>(
            un->op,
            clone_expr(*un->operand, var_subst),
            un->location
        );
        res->evaluated_type = un->evaluated_type;
        return res;
    }

    if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        auto res = std::make_unique<AssignExpr>(
            clone_expr(*assign->target, var_subst),
            clone_expr(*assign->value, var_subst),
            assign->location
        );
        res->evaluated_type = assign->evaluated_type;
        return res;
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        std::vector<std::unique_ptr<Expression>> args;
        for (const auto& a : call->arguments) {
            args.push_back(clone_expr(*a, var_subst));
        }
        auto res = std::make_unique<CallExpr>(call->callee, std::move(args), call->location, call->type_args);
        res->evaluated_type = call->evaluated_type;
        return res;
    }

    if (auto* if_expr = dynamic_cast<const IfExpr*>(&expr)) {
        auto cond = clone_expr(*if_expr->condition, var_subst);
        auto then_b = clone_block(*if_expr->then_branch, var_subst);
        std::unique_ptr<BlockExpr> else_b = if_expr->else_branch ? clone_block(*if_expr->else_branch, var_subst) : nullptr;
        auto res = std::make_unique<IfExpr>(std::move(cond), std::move(then_b), std::move(else_b), if_expr->location);
        res->evaluated_type = if_expr->evaluated_type;
        return res;
    }

    if (auto* while_expr = dynamic_cast<const WhileExpr*>(&expr)) {
        auto cond = clone_expr(*while_expr->condition, var_subst);
        auto body = clone_block(*while_expr->body, var_subst);
        auto res = std::make_unique<WhileExpr>(std::move(cond), std::move(body), while_expr->location);
        res->evaluated_type = while_expr->evaluated_type;
        return res;
    }

    if (auto* ret = dynamic_cast<const ReturnExpr*>(&expr)) {
        auto val = ret->value ? clone_expr(*ret->value, var_subst) : nullptr;
        auto res = std::make_unique<ReturnExpr>(std::move(val), ret->location);
        res->evaluated_type = ret->evaluated_type;
        return res;
    }

    if (auto* s_init = dynamic_cast<const StructInitExpr*>(&expr)) {
        std::vector<StructFieldInit> fields;
        for (const auto& f : s_init->fields) {
            fields.push_back(StructFieldInit{f.field_name, clone_expr(*f.value, var_subst), f.location});
        }
        auto res = std::make_unique<StructInitExpr>(s_init->struct_name, std::move(fields), s_init->location, s_init->type_args);
        res->evaluated_type = s_init->evaluated_type;
        return res;
    }

    if (auto* fa = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        auto res = std::make_unique<FieldAccessExpr>(clone_expr(*fa->target, var_subst), fa->field_name, fa->location);
        res->evaluated_type = fa->evaluated_type;
        return res;
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&expr)) {
        auto recv = clone_expr(*mc->receiver, var_subst);
        std::vector<std::unique_ptr<Expression>> args;
        for (const auto& a : mc->arguments) {
            args.push_back(clone_expr(*a, var_subst));
        }
        auto res = std::make_unique<MethodCallExpr>(std::move(recv), mc->method_name, std::move(args), mc->location, mc->type_args);
        res->resolved_self_kind = mc->resolved_self_kind;
        res->resolved_callee = mc->resolved_callee;
        res->evaluated_type = mc->evaluated_type;
        return res;
    }

    if (auto* ev = dynamic_cast<const EnumVariantExpr*>(&expr)) {
        auto payload = ev->payload ? clone_expr(*ev->payload, var_subst) : nullptr;
        auto res = std::make_unique<EnumVariantExpr>(ev->enum_name, ev->variant_name, std::move(payload), ev->location);
        res->evaluated_type = ev->evaluated_type;
        return res;
    }

    if (auto* match_expr = dynamic_cast<const MatchExpr*>(&expr)) {
        auto tgt = clone_expr(*match_expr->target, var_subst);
        std::vector<MatchArm> arms;
        for (const auto& a : match_expr->arms) {
            arms.push_back(MatchArm{a.pattern, clone_expr(*a.body, var_subst), a.location});
        }
        auto res = std::make_unique<MatchExpr>(std::move(tgt), std::move(arms), match_expr->location);
        res->evaluated_type = match_expr->evaluated_type;
        return res;
    }

    if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        std::vector<std::unique_ptr<Expression>> elems;
        for (const auto& el : arr->elements) {
            elems.push_back(clone_expr(*el, var_subst));
        }
        auto res = std::make_unique<ArrayLiteralExpr>(std::move(elems), arr->location);
        res->evaluated_type = arr->evaluated_type;
        return res;
    }

    if (auto* rep = dynamic_cast<const ArrayRepeatExpr*>(&expr)) {
        auto res = std::make_unique<ArrayRepeatExpr>(clone_expr(*rep->value, var_subst), rep->count, rep->location);
        res->evaluated_type = rep->evaluated_type;
        return res;
    }

    if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        auto res = std::make_unique<IndexExpr>(clone_expr(*idx->target, var_subst), clone_expr(*idx->index, var_subst), idx->location);
        res->evaluated_type = idx->evaluated_type;
        return res;
    }

    if (auto* range = dynamic_cast<const RangeExpr*>(&expr)) {
        auto res = std::make_unique<RangeExpr>(clone_expr(*range->start, var_subst), clone_expr(*range->end, var_subst), range->location);
        res->evaluated_type = range->evaluated_type;
        return res;
    }

    if (auto* for_expr = dynamic_cast<const ForExpr*>(&expr)) {
        auto sub = var_subst;
        sub.erase(for_expr->var_name);
        auto res = std::make_unique<ForExpr>(for_expr->var_name, clone_expr(*for_expr->iterable, var_subst),
                                             clone_block(*for_expr->body, sub), for_expr->location);
        res->evaluated_type = for_expr->evaluated_type;
        return res;
    }

    if (auto* block = dynamic_cast<const BlockExpr*>(&expr)) {
        return clone_block(*block, var_subst);
    }

    return nullptr;
}

std::unique_ptr<Statement> Optimizer::clone_stmt(
    const Statement& stmt,
    const std::map<std::string, const Expression*>& var_subst
) {
    if (auto* let = dynamic_cast<const LetStmt*>(&stmt)) {
        auto init = let->initializer ? clone_expr(*let->initializer, var_subst) : nullptr;
        return std::make_unique<LetStmt>(let->var_name, let->is_mutable, let->declared_type, std::move(init), let->location);
    }
    if (auto* expr_stmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        auto e = expr_stmt->expr ? clone_expr(*expr_stmt->expr, var_subst) : nullptr;
        return std::make_unique<ExprStmt>(std::move(e), expr_stmt->location);
    }
    return nullptr;
}

std::unique_ptr<BlockExpr> Optimizer::clone_block(
    const BlockExpr& block,
    const std::map<std::string, const Expression*>& var_subst
) {
    auto res = std::make_unique<BlockExpr>(block.location);
    res->evaluated_type = block.evaluated_type;
    for (const auto& s : block.statements) {
        if (s) {
            res->statements.push_back(clone_stmt(*s, var_subst));
        }
    }
    if (block.trailing_expr) {
        res->trailing_expr = clone_expr(*block.trailing_expr, var_subst);
    }
    return res;
}

void Optimizer::optimize_statement(std::unique_ptr<Statement>& stmt) {
    if (auto* let = dynamic_cast<LetStmt*>(stmt.get())) {
        if (let->initializer) {
            let->initializer = fold_expression(std::move(let->initializer));
        }
    } else if (auto* expr_stmt = dynamic_cast<ExprStmt*>(stmt.get())) {
        if (expr_stmt->expr) {
            expr_stmt->expr = fold_expression(std::move(expr_stmt->expr));
        }
    }
}

// Recursively simplifies subexpressions via constant folding.
// When operands are known constants (IntegerLiteralExpr, BoolLiteralExpr),
// performs arithmetic and relational operations at compile time.
std::unique_ptr<Expression> Optimizer::fold_expression(std::unique_ptr<Expression> expr) {
    if (!expr) return nullptr;

    if (auto* bin = dynamic_cast<BinaryExpr*>(expr.get())) {
        bin->left = fold_expression(std::move(bin->left));
        bin->right = fold_expression(std::move(bin->right));

        auto* l_int = dynamic_cast<IntegerLiteralExpr*>(bin->left.get());
        auto* r_int = dynamic_cast<IntegerLiteralExpr*>(bin->right.get());

        if (l_int && r_int) {
            switch (bin->op) {
                case BinaryOp::Add: {
                    auto res = std::make_unique<IntegerLiteralExpr>(l_int->value + r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Sub: {
                    auto res = std::make_unique<IntegerLiteralExpr>(l_int->value - r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Mul: {
                    auto res = std::make_unique<IntegerLiteralExpr>(l_int->value * r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Div: {
                    if (r_int->value != 0) {
                        auto res = std::make_unique<IntegerLiteralExpr>(l_int->value / r_int->value, bin->location);
                        res->evaluated_type = bin->evaluated_type;
                        return res;
                    }
                    break;
                }
                case BinaryOp::Equal: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_int->value == r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::NotEqual: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_int->value != r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Less: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_int->value < r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::LessEqual: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_int->value <= r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Greater: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_int->value > r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::GreaterEqual: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_int->value >= r_int->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
            }
        }

        auto* l_fl = dynamic_cast<FloatLiteralExpr*>(bin->left.get());
        auto* r_fl = dynamic_cast<FloatLiteralExpr*>(bin->right.get());

        if (l_fl && r_fl) {
            switch (bin->op) {
                case BinaryOp::Add: {
                    auto res = std::make_unique<FloatLiteralExpr>(l_fl->value + r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Sub: {
                    auto res = std::make_unique<FloatLiteralExpr>(l_fl->value - r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Mul: {
                    auto res = std::make_unique<FloatLiteralExpr>(l_fl->value * r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Div: {
                    if (r_fl->value != 0.0) {
                        auto res = std::make_unique<FloatLiteralExpr>(l_fl->value / r_fl->value, bin->location);
                        res->evaluated_type = bin->evaluated_type;
                        return res;
                    }
                    break;
                }
                case BinaryOp::Equal: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_fl->value == r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::NotEqual: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_fl->value != r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Less: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_fl->value < r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::LessEqual: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_fl->value <= r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::Greater: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_fl->value > r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
                case BinaryOp::GreaterEqual: {
                    auto res = std::make_unique<BoolLiteralExpr>(l_fl->value >= r_fl->value, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
            }
        }

        auto* l_bool = dynamic_cast<BoolLiteralExpr*>(bin->left.get());
        auto* r_bool = dynamic_cast<BoolLiteralExpr*>(bin->right.get());
        if (l_bool && r_bool) {
            if (bin->op == BinaryOp::Equal) {
                auto res = std::make_unique<BoolLiteralExpr>(l_bool->value == r_bool->value, bin->location);
                res->evaluated_type = bin->evaluated_type;
                return res;
            } else if (bin->op == BinaryOp::NotEqual) {
                auto res = std::make_unique<BoolLiteralExpr>(l_bool->value != r_bool->value, bin->location);
                res->evaluated_type = bin->evaluated_type;
                return res;
            }
        }

        // Algebraic identities and annihilators:
        if (bin->op == BinaryOp::Add) {
            if (auto* r_int = dynamic_cast<IntegerLiteralExpr*>(bin->right.get())) {
                if (r_int->value == 0) {
                    return std::move(bin->left);
                }
            }
            if (auto* l_int = dynamic_cast<IntegerLiteralExpr*>(bin->left.get())) {
                if (l_int->value == 0) {
                    return std::move(bin->right);
                }
            }
            if (auto* r_fl = dynamic_cast<FloatLiteralExpr*>(bin->right.get())) {
                if (r_fl->value == 0.0) {
                    return std::move(bin->left);
                }
            }
            if (auto* l_fl = dynamic_cast<FloatLiteralExpr*>(bin->left.get())) {
                if (l_fl->value == 0.0) {
                    return std::move(bin->right);
                }
            }
        }
        if (bin->op == BinaryOp::Sub) {
            if (auto* r_int = dynamic_cast<IntegerLiteralExpr*>(bin->right.get())) {
                if (r_int->value == 0) {
                    return std::move(bin->left);
                }
            }
            if (auto* r_fl = dynamic_cast<FloatLiteralExpr*>(bin->right.get())) {
                if (r_fl->value == 0.0) {
                    return std::move(bin->left);
                }
            }
        }
        if (bin->op == BinaryOp::Mul) {
            if (auto* r_int = dynamic_cast<IntegerLiteralExpr*>(bin->right.get())) {
                if (r_int->value == 1) {
                    return std::move(bin->left);
                }
                if (r_int->value == 0) {
                    auto res = std::make_unique<IntegerLiteralExpr>(0, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
            }
            if (auto* l_int = dynamic_cast<IntegerLiteralExpr*>(bin->left.get())) {
                if (l_int->value == 1) {
                    return std::move(bin->right);
                }
                if (l_int->value == 0) {
                    auto res = std::make_unique<IntegerLiteralExpr>(0, bin->location);
                    res->evaluated_type = bin->evaluated_type;
                    return res;
                }
            }
            if (auto* r_fl = dynamic_cast<FloatLiteralExpr*>(bin->right.get())) {
                if (r_fl->value == 1.0) {
                    return std::move(bin->left);
                }
            }
            if (auto* l_fl = dynamic_cast<FloatLiteralExpr*>(bin->left.get())) {
                if (l_fl->value == 1.0) {
                    return std::move(bin->right);
                }
            }
        }
        if (bin->op == BinaryOp::Div) {
            if (auto* r_int = dynamic_cast<IntegerLiteralExpr*>(bin->right.get())) {
                if (r_int->value == 1) {
                    return std::move(bin->left);
                }
            }
            if (auto* r_fl = dynamic_cast<FloatLiteralExpr*>(bin->right.get())) {
                if (r_fl->value == 1.0) {
                    return std::move(bin->left);
                }
            }
        }

        return expr;
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(expr.get())) {
        un->operand = fold_expression(std::move(un->operand));
        if (un->op == UnaryOp::Negate) {
            // Double negation: -(-x) -> x
            if (auto* inner_un = dynamic_cast<UnaryExpr*>(un->operand.get())) {
                if (inner_un->op == UnaryOp::Negate) {
                    return std::move(inner_un->operand);
                }
            }
            if (auto* int_lit = dynamic_cast<IntegerLiteralExpr*>(un->operand.get())) {
                auto res = std::make_unique<IntegerLiteralExpr>(-int_lit->value, un->location);
                res->evaluated_type = un->evaluated_type;
                return res;
            }
            if (auto* fl_lit = dynamic_cast<FloatLiteralExpr*>(un->operand.get())) {
                auto res = std::make_unique<FloatLiteralExpr>(-fl_lit->value, un->location);
                res->evaluated_type = un->evaluated_type;
                return res;
            }
        } else if (un->op == UnaryOp::Not) {
            // Double logical not: !(!x) -> x
            if (auto* inner_un = dynamic_cast<UnaryExpr*>(un->operand.get())) {
                if (inner_un->op == UnaryOp::Not) {
                    return std::move(inner_un->operand);
                }
            }
            if (auto* bool_lit = dynamic_cast<BoolLiteralExpr*>(un->operand.get())) {
                auto res = std::make_unique<BoolLiteralExpr>(!bool_lit->value, un->location);
                res->evaluated_type = un->evaluated_type;
                return res;
            }
        }
        return expr;
    }

    if (auto* assign = dynamic_cast<AssignExpr*>(expr.get())) {
        assign->value = fold_expression(std::move(assign->value));
        return expr;
    }

    if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
        for (auto& a : call->arguments) {
            a = fold_expression(std::move(a));
        }
        if (auto inlined = try_inline_call(*call)) {
            changed_ = true;
            return fold_expression(std::move(inlined));
        }
        return expr;
    }

    if (auto* if_expr = dynamic_cast<IfExpr*>(expr.get())) {
        if_expr->condition = fold_expression(std::move(if_expr->condition));
        if (auto* cond_bool = dynamic_cast<BoolLiteralExpr*>(if_expr->condition.get())) {
            changed_ = true;
            if (cond_bool->value) {
                // Condition is statically true: eliminate dead else branch
                if_expr->else_branch = nullptr;
                if (if_expr->then_branch) {
                    optimize_block(*if_expr->then_branch);
                    if (if_expr->then_branch->statements.empty() && if_expr->then_branch->trailing_expr) {
                        return std::move(if_expr->then_branch->trailing_expr);
                    }
                }
            } else {
                // Condition is statically false: prune then branch statements
                if (if_expr->then_branch) {
                    if_expr->then_branch->statements.clear();
                    if_expr->then_branch->trailing_expr = nullptr;
                }
                if (if_expr->else_branch) {
                    optimize_block(*if_expr->else_branch);
                    if (if_expr->else_branch->statements.empty() && if_expr->else_branch->trailing_expr) {
                        return std::move(if_expr->else_branch->trailing_expr);
                    }
                }
            }
            return expr;
        }
        if (if_expr->then_branch) optimize_block(*if_expr->then_branch);
        if (if_expr->else_branch) optimize_block(*if_expr->else_branch);
        return expr;
    }

    if (auto* while_expr = dynamic_cast<WhileExpr*>(expr.get())) {
        while_expr->condition = fold_expression(std::move(while_expr->condition));
        if (auto* cond_bool = dynamic_cast<BoolLiteralExpr*>(while_expr->condition.get())) {
            if (!cond_bool->value) {
                // while false loop body is dead code: prune statements
                if (while_expr->body) {
                    while_expr->body->statements.clear();
                    while_expr->body->trailing_expr = nullptr;
                }
            }
        }
        if (while_expr->body) optimize_block(*while_expr->body);
        return expr;
    }

    if (auto* for_expr = dynamic_cast<ForExpr*>(expr.get())) {
        for_expr->iterable = fold_expression(std::move(for_expr->iterable));
        if (for_expr->body) optimize_block(*for_expr->body);
        return expr;
    }

    if (auto* ret = dynamic_cast<ReturnExpr*>(expr.get())) {
        if (ret->value) {
            ret->value = fold_expression(std::move(ret->value));
        }
        return expr;
    }

    if (auto* arr = dynamic_cast<ArrayLiteralExpr*>(expr.get())) {
        for (auto& el : arr->elements) {
            el = fold_expression(std::move(el));
        }
        return expr;
    }

    if (auto* rep = dynamic_cast<ArrayRepeatExpr*>(expr.get())) {
        rep->value = fold_expression(std::move(rep->value));
        return expr;
    }

    if (auto* idx = dynamic_cast<IndexExpr*>(expr.get())) {
        idx->target = fold_expression(std::move(idx->target));
        idx->index = fold_expression(std::move(idx->index));
        return expr;
    }

    if (auto* block = dynamic_cast<BlockExpr*>(expr.get())) {
        optimize_block(*block);
        return expr;
    }

    return expr;
}

} // namespace oxide
