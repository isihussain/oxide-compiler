#pragma once

#include "ast.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief AST-level optimization transformation pass.
 *
 * Implements compile-time simplifications prior to backend code generation:
 *  1. Constant Folding:
 *     - Evaluates binary arithmetic operators (+, -, *, /) on IntegerLiteralExpr nodes.
 *     - Evaluates relational comparisons (==, !=, <, <=, >, >=) to BoolLiteralExpr nodes.
 *     - Evaluates unary operations (- for integers, ! for booleans).
 *     - Evaluates algebraic identities (x + 0, x - 0, x * 1, x * 0, x / 1) and annihilators.
 *     - Evaluates double negations (-(-x) -> x, !(!x) -> x).
 *  2. Dead Code Elimination (DCE):
 *     - Detects unconditional ReturnExpr statements in BlockExpr nodes and prunes all
 *       subsequent dead statements within the block.
 *     - Prunes unreachable branches in IfExpr nodes when conditions evaluate to constants.
 *     - Prunes unexecutable loop bodies in WhileExpr nodes when conditions evaluate to false.
 *  3. Constant-Bounded Loop Unrolling:
 *     - Unrolls for loops over compile-time constant RangeExpr bounds (trip count <= 8).
 *     - Completely eliminates dead loops when start >= end (trip count == 0).
 *     - Replaces loop iteration with scoped block statements and propagates constant loop index.
 *  4. Pure Function Inline Expansion:
 *     - Expands calls to leaf functions with single return expressions or #[inline] annotations.
 *     - Directly substitutes parameter identifiers with argument expressions when arguments are pure.
 *     - Triggers recursive constant folding on inlined function call sites.
 */
class Optimizer {
public:
    explicit Optimizer(Program& program);

    /**
     * @brief Traverses the program AST and applies all optimization transformations.
     */
    void run();

private:
    void collect_inline_candidates();
    bool is_inlinable_function(const Function& fn);
    const Expression* get_return_expression(const Function& fn);
    bool is_pure_argument(const Expression& expr);
    bool is_leaf_expression(const Expression& expr, const std::string& self_name);
    int count_ast_nodes(const Expression& expr);

    void optimize_function(Function& func);
    void optimize_block(BlockExpr& block);
    void optimize_statement(std::unique_ptr<Statement>& stmt);
    std::unique_ptr<Expression> fold_expression(std::unique_ptr<Expression> expr);

    bool try_unroll_for_loop(ForExpr& for_expr, std::vector<std::unique_ptr<Statement>>& out);
    std::unique_ptr<Expression> try_inline_call(const CallExpr& call);

    std::unique_ptr<Expression> clone_expr(const Expression& expr, const std::map<std::string, const Expression*>& var_subst);
    std::unique_ptr<Statement> clone_stmt(const Statement& stmt, const std::map<std::string, const Expression*>& var_subst);
    std::unique_ptr<BlockExpr> clone_block(const BlockExpr& block, const std::map<std::string, const Expression*>& var_subst);

    Program& program_;
    std::map<std::string, const Function*> inline_candidates_;
    bool changed_ = false;
};

} // namespace oxide
