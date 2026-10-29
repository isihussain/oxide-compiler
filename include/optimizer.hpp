#pragma once

#include "ast.hpp"

namespace oxide {

/**
 * @brief AST-level optimization transformation pass.
 *
 * Implements compile-time simplifications prior to backend code generation:
 *  1. Constant Folding:
 *     - Evaluates binary arithmetic operators (+, -, *, /) on IntegerLiteralExpr nodes.
 *     - Evaluates relational comparisons (==, !=, <, <=, >, >=) to BoolLiteralExpr nodes.
 *     - Evaluates unary operations (- for integers, ! for booleans).
 *  2. Dead Code Elimination (DCE):
 *     - Detects unconditional ReturnExpr statements in BlockExpr nodes and prunes all
 *       subsequent dead statements within the block.
 */
class Optimizer {
public:
    explicit Optimizer(Program& program);

    /**
     * @brief Traverses the program AST and applies all optimization transformations.
     */
    void run();

private:
    void optimize_function(Function& func);
    void optimize_block(BlockExpr& block);
    void optimize_statement(std::unique_ptr<Statement>& stmt);
    std::unique_ptr<Expression> fold_expression(std::unique_ptr<Expression> expr);

    Program& program_;
};

} // namespace oxide
