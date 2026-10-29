#pragma once

#include "ast.hpp"

namespace oxide {

class Optimizer {
public:
    explicit Optimizer(Program& program);
    void run();

private:
    void optimize_function(Function& func);
    void optimize_block(BlockExpr& block);
    void optimize_statement(std::unique_ptr<Statement>& stmt);
    std::unique_ptr<Expression> fold_expression(std::unique_ptr<Expression> expr);

    Program& program_;
};

} // namespace oxide
