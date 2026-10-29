#include "optimizer.hpp"

namespace oxide {

Optimizer::Optimizer(Program& program)
    : program_(program) {}

void Optimizer::run() {
    for (auto& fn : program_.functions) {
        optimize_function(*fn);
    }
    for (auto& ib : program_.impl_blocks) {
        for (auto& fn : ib->methods) {
            optimize_function(*fn);
        }
    }
}

void Optimizer::optimize_function(Function& func) {
    if (func.body) {
        optimize_block(*func.body);
    }
}

// Dead code elimination on block statements:
// Once an unconditional return statement is encountered, all subsequent statements
// in the same lexical block are statically pruned from the AST.
void Optimizer::optimize_block(BlockExpr& block) {
    std::vector<std::unique_ptr<Statement>> new_stmts;
    bool returned = false;

    for (auto& s : block.statements) {
        if (returned) {
            // Dead code elimination: skip statements following an unconditional return
            continue;
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
        block.trailing_expr = nullptr;
    } else if (block.trailing_expr) {
        block.trailing_expr = fold_expression(std::move(block.trailing_expr));
    }
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

        return expr;
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(expr.get())) {
        un->operand = fold_expression(std::move(un->operand));
        if (un->op == UnaryOp::Negate) {
            if (auto* int_lit = dynamic_cast<IntegerLiteralExpr*>(un->operand.get())) {
                auto res = std::make_unique<IntegerLiteralExpr>(-int_lit->value, un->location);
                res->evaluated_type = un->evaluated_type;
                return res;
            }
        } else if (un->op == UnaryOp::Not) {
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
        return expr;
    }

    if (auto* if_expr = dynamic_cast<IfExpr*>(expr.get())) {
        if_expr->condition = fold_expression(std::move(if_expr->condition));
        if (if_expr->then_branch) optimize_block(*if_expr->then_branch);
        if (if_expr->else_branch) optimize_block(*if_expr->else_branch);
        return expr;
    }

    if (auto* while_expr = dynamic_cast<WhileExpr*>(expr.get())) {
        while_expr->condition = fold_expression(std::move(while_expr->condition));
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
