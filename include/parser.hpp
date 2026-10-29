#pragma once

#include "ast.hpp"
#include "token.hpp"
#include <string>
#include <vector>

namespace oxide {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Program> parse_program();

private:
    const Token& peek() const;
    const Token& previous() const;
    bool is_at_end() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& error_message);

    std::unique_ptr<Function> parse_function();
    std::unique_ptr<StructDef> parse_struct_definition();
    std::unique_ptr<ExternBlock> parse_extern_block();
    Parameter parse_parameter();
    Type parse_type();

    std::unique_ptr<BlockExpr> parse_block();
    std::unique_ptr<Statement> parse_statement();
    std::unique_ptr<Statement> parse_let_statement();

    std::unique_ptr<Expression> parse_expression();
    std::unique_ptr<Expression> parse_assignment();
    std::unique_ptr<Expression> parse_equality();
    std::unique_ptr<Expression> parse_comparison();
    std::unique_ptr<Expression> parse_term();
    std::unique_ptr<Expression> parse_factor();
    std::unique_ptr<Expression> parse_unary();
    std::unique_ptr<Expression> parse_primary();
    std::unique_ptr<IfExpr> parse_if_expression();
    std::unique_ptr<WhileExpr> parse_while_expression();
    std::unique_ptr<ReturnExpr> parse_return_expression();

    std::vector<Token> tokens_;
    size_t cursor_ = 0;
};

} // namespace oxide
