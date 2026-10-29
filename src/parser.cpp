#include "parser.hpp"
#include <iostream>
#include <stdexcept>

namespace oxide {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::peek() const {
    if (cursor_ >= tokens_.size()) {
        return tokens_.back(); // Eof
    }
    return tokens_[cursor_];
}

const Token& Parser::previous() const {
    if (cursor_ == 0) return tokens_[0];
    return tokens_[cursor_ - 1];
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::Eof;
}

const Token& Parser::advance() {
    if (!is_at_end()) {
        cursor_++;
    }
    return previous();
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& error_message) {
    if (check(type)) {
        return advance();
    }
    const auto& tok = peek();
    std::string full_msg = "Parse error at [" + std::to_string(tok.location.line) + ":" +
                           std::to_string(tok.location.column) + "]: " + error_message +
                           ", found '" + tok.text + "'";
    throw std::runtime_error(full_msg);
}

std::unique_ptr<Program> Parser::parse_program() {
    auto program = std::make_unique<Program>();
    program->location = peek().location;

    while (!is_at_end()) {
        if (check(TokenType::KwFn)) {
            program->functions.push_back(parse_function());
        } else if (check(TokenType::KwStruct)) {
            program->structs.push_back(parse_struct_definition());
        } else {
            throw std::runtime_error("Unexpected token at top level: " + peek().to_string());
        }
    }
    return program;
}

std::unique_ptr<StructDef> Parser::parse_struct_definition() {
    SourceLocation loc = consume(TokenType::KwStruct, "Expected 'struct'").location;
    std::string name = consume(TokenType::Identifier, "Expected struct name").text;

    consume(TokenType::LBrace, "Expected '{' after struct name");
    std::vector<StructField> fields;
    while (!check(TokenType::RBrace) && !is_at_end()) {
        const Token& field_tok = consume(TokenType::Identifier, "Expected field name");
        consume(TokenType::Colon, "Expected ':' after field name");
        Type type = parse_type();
        fields.push_back(StructField{field_tok.text, type, field_tok.location});
        if (!match(TokenType::Comma)) {
            break;
        }
    }
    consume(TokenType::RBrace, "Expected '}' to close struct definition");
    return std::make_unique<StructDef>(name, std::move(fields), loc);
}

std::unique_ptr<Function> Parser::parse_function() {
    SourceLocation loc = consume(TokenType::KwFn, "Expected 'fn'").location;
    std::string name = consume(TokenType::Identifier, "Expected function name").text;

    consume(TokenType::LParen, "Expected '(' after function name");
    std::vector<Parameter> params;
    if (!check(TokenType::RParen)) {
        do {
            params.push_back(parse_parameter());
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')' after parameters");

    Type return_type{BaseTypeKind::Unit, false, false};
    if (match(TokenType::Arrow)) {
        return_type = parse_type();
    }

    auto body = parse_block();
    return std::make_unique<Function>(name, std::move(params), return_type, std::move(body), loc);
}

Parameter Parser::parse_parameter() {
    bool is_mut = match(TokenType::KwMut);
    const Token& name_tok = consume(TokenType::Identifier, "Expected parameter name");
    consume(TokenType::Colon, "Expected ':' after parameter name");
    Type type = parse_type();
    return Parameter{name_tok.text, type, is_mut, name_tok.location};
}

Type Parser::parse_type() {
    Type type;
    if (match(TokenType::Amp)) {
        type.is_ref = true;
        if (match(TokenType::KwMut)) {
            type.is_mut_ref = true;
        }
    }

    if (match(TokenType::TypeI32)) {
        type.kind = BaseTypeKind::I32;
    } else if (match(TokenType::TypeBool)) {
        type.kind = BaseTypeKind::Bool;
    } else if (match(TokenType::LParen)) {
        consume(TokenType::RParen, "Expected ')' for unit type '()'");
        type.kind = BaseTypeKind::Unit;
    } else if (match(TokenType::Identifier)) {
        type.kind = BaseTypeKind::Named;
        type.name = previous().text;
    } else {
        throw std::runtime_error("Expected type name at [" + std::to_string(peek().location.line) + ":" +
                                 std::to_string(peek().location.column) + "], found: " + peek().text);
    }
    return type;
}

std::unique_ptr<BlockExpr> Parser::parse_block() {
    SourceLocation loc = consume(TokenType::LBrace, "Expected '{' to start block").location;
    auto block = std::make_unique<BlockExpr>(loc);

    while (!check(TokenType::RBrace) && !is_at_end()) {
        if (check(TokenType::KwLet)) {
            block->statements.push_back(parse_let_statement());
        } else {
            // Can be an expression statement or a trailing expression
            auto expr = parse_expression();
            if (match(TokenType::Semicolon)) {
                block->statements.push_back(std::make_unique<ExprStmt>(std::move(expr), expr->location));
            } else if (check(TokenType::RBrace)) {
                // Trailing expression!
                block->trailing_expr = std::move(expr);
                break;
            } else if (dynamic_cast<IfExpr*>(expr.get()) ||
                       dynamic_cast<WhileExpr*>(expr.get()) ||
                       dynamic_cast<BlockExpr*>(expr.get())) {
                // In Rust, expressions ending with '}' do not require a semicolon
                block->statements.push_back(std::make_unique<ExprStmt>(std::move(expr), expr->location));
            } else {
                throw std::runtime_error("Expected ';' after statement at [" +
                                         std::to_string(peek().location.line) + ":" +
                                         std::to_string(peek().location.column) + "]");
            }
        }
    }

    consume(TokenType::RBrace, "Expected '}' to close block");
    return block;
}

std::unique_ptr<Statement> Parser::parse_let_statement() {
    SourceLocation loc = consume(TokenType::KwLet, "Expected 'let'").location;
    bool is_mut = match(TokenType::KwMut);
    std::string var_name = consume(TokenType::Identifier, "Expected variable name").text;

    Type declared_type{BaseTypeKind::Unit, false, false};
    if (match(TokenType::Colon)) {
        declared_type = parse_type();
    }

    std::unique_ptr<Expression> initializer = nullptr;
    if (match(TokenType::Eq)) {
        initializer = parse_expression();
    }

    consume(TokenType::Semicolon, "Expected ';' after let statement");
    return std::make_unique<LetStmt>(var_name, is_mut, declared_type, std::move(initializer), loc);
}

std::unique_ptr<Expression> Parser::parse_expression() {
    return parse_assignment();
}

std::unique_ptr<Expression> Parser::parse_assignment() {
    auto expr = parse_equality();

    if (match(TokenType::Eq)) {
        SourceLocation loc = previous().location;
        bool is_valid_lvalue = (dynamic_cast<VariableExpr*>(expr.get()) != nullptr) ||
                               (dynamic_cast<FieldAccessExpr*>(expr.get()) != nullptr) ||
                               (dynamic_cast<UnaryExpr*>(expr.get()) != nullptr &&
                                dynamic_cast<UnaryExpr*>(expr.get())->op == UnaryOp::Deref);

        if (!is_valid_lvalue) {
            throw std::runtime_error("Invalid assignment target at [" +
                                     std::to_string(loc.line) + ":" + std::to_string(loc.column) + "]");
        }
        auto value = parse_assignment();
        return std::make_unique<AssignExpr>(std::move(expr), std::move(value), loc);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_equality() {
    auto expr = parse_comparison();

    while (match(TokenType::EqEq) || match(TokenType::NotEq)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op = (op_type == TokenType::EqEq) ? BinaryOp::Equal : BinaryOp::NotEqual;
        auto right = parse_comparison();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), loc);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_comparison() {
    auto expr = parse_term();

    while (match(TokenType::Lt) || match(TokenType::LtEq) ||
           match(TokenType::Gt) || match(TokenType::GtEq)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op;
        switch (op_type) {
            case TokenType::Lt: op = BinaryOp::Less; break;
            case TokenType::LtEq: op = BinaryOp::LessEqual; break;
            case TokenType::Gt: op = BinaryOp::Greater; break;
            case TokenType::GtEq: op = BinaryOp::GreaterEqual; break;
            default: break;
        }
        auto right = parse_term();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), loc);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_term() {
    auto expr = parse_factor();

    while (match(TokenType::Plus) || match(TokenType::Minus)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op = (op_type == TokenType::Plus) ? BinaryOp::Add : BinaryOp::Sub;
        auto right = parse_factor();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), loc);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_factor() {
    auto expr = parse_unary();

    while (match(TokenType::Star) || match(TokenType::Slash)) {
        TokenType op_type = previous().type;
        SourceLocation loc = previous().location;
        BinaryOp op = (op_type == TokenType::Star) ? BinaryOp::Mul : BinaryOp::Div;
        auto right = parse_unary();
        expr = std::make_unique<BinaryExpr>(op, std::move(expr), std::move(right), loc);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_unary() {
    if (match(TokenType::Minus)) {
        SourceLocation loc = previous().location;
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(UnaryOp::Negate, std::move(operand), loc);
    }
    if (match(TokenType::Exclamation)) {
        SourceLocation loc = previous().location;
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(UnaryOp::Not, std::move(operand), loc);
    }
    if (match(TokenType::Amp)) {
        SourceLocation loc = previous().location;
        if (match(TokenType::KwMut)) {
            auto operand = parse_unary();
            return std::make_unique<UnaryExpr>(UnaryOp::RefMut, std::move(operand), loc);
        }
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(UnaryOp::Ref, std::move(operand), loc);
    }
    if (match(TokenType::Star)) {
        SourceLocation loc = previous().location;
        auto operand = parse_unary();
        return std::make_unique<UnaryExpr>(UnaryOp::Deref, std::move(operand), loc);
    }

    auto expr = parse_primary();
    while (match(TokenType::Dot)) {
        const Token& field_tok = consume(TokenType::Identifier, "Expected field name after '.'");
        expr = std::make_unique<FieldAccessExpr>(std::move(expr), field_tok.text, field_tok.location);
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parse_primary() {
    SourceLocation loc = peek().location;

    if (match(TokenType::LiteralInt)) {
        return std::make_unique<IntegerLiteralExpr>(previous().int_value, loc);
    }

    if (match(TokenType::LiteralBool)) {
        return std::make_unique<BoolLiteralExpr>(previous().bool_value, loc);
    }

    if (match(TokenType::KwIf)) {
        return parse_if_expression();
    }

    if (match(TokenType::KwWhile)) {
        return parse_while_expression();
    }

    if (match(TokenType::KwReturn)) {
        return parse_return_expression();
    }

    if (match(TokenType::Identifier)) {
        std::string name = previous().text;
        // Function call: ident(...)
        if (match(TokenType::LParen)) {
            std::vector<std::unique_ptr<Expression>> args;
            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after call arguments");
            return std::make_unique<CallExpr>(name, std::move(args), loc);
        }
        // Struct literal: Name { field: value, ... }
        if (check(TokenType::LBrace) && cursor_ + 2 < tokens_.size() &&
            tokens_[cursor_ + 1].type == TokenType::Identifier &&
            tokens_[cursor_ + 2].type == TokenType::Colon) {
            advance(); // consume '{'
            std::vector<StructFieldInit> inits;
            while (!check(TokenType::RBrace) && !is_at_end()) {
                const Token& f_tok = consume(TokenType::Identifier, "Expected field name in struct literal");
                consume(TokenType::Colon, "Expected ':' after field name");
                auto val = parse_expression();
                inits.push_back(StructFieldInit{f_tok.text, std::move(val), f_tok.location});
                if (!match(TokenType::Comma)) {
                    break;
                }
            }
            consume(TokenType::RBrace, "Expected '}' to close struct literal");
            return std::make_unique<StructInitExpr>(name, std::move(inits), loc);
        }
        return std::make_unique<VariableExpr>(name, loc);
    }

    if (match(TokenType::LParen)) {
        auto expr = parse_expression();
        consume(TokenType::RParen, "Expected ')' after parenthesized expression");
        return expr;
    }

    if (check(TokenType::LBrace)) {
        return parse_block();
    }

    throw std::runtime_error("Unexpected token in expression at [" +
                             std::to_string(loc.line) + ":" + std::to_string(loc.column) +
                             "]: '" + peek().text + "'");
}

std::unique_ptr<IfExpr> Parser::parse_if_expression() {
    SourceLocation loc = previous().location;
    auto cond = parse_expression();
    auto then_branch = parse_block();
    std::unique_ptr<BlockExpr> else_branch = nullptr;

    if (match(TokenType::KwElse)) {
        if (match(TokenType::KwIf)) {
            // else if -> nest as single-statement block
            auto nested_if = parse_if_expression();
            else_branch = std::make_unique<BlockExpr>(nested_if->location);
            else_branch->trailing_expr = std::move(nested_if);
        } else {
            else_branch = parse_block();
        }
    }

    return std::make_unique<IfExpr>(std::move(cond), std::move(then_branch), std::move(else_branch), loc);
}

std::unique_ptr<WhileExpr> Parser::parse_while_expression() {
    SourceLocation loc = previous().location;
    auto cond = parse_expression();
    auto body = parse_block();
    return std::make_unique<WhileExpr>(std::move(cond), std::move(body), loc);
}

std::unique_ptr<ReturnExpr> Parser::parse_return_expression() {
    SourceLocation loc = previous().location;
    std::unique_ptr<Expression> val = nullptr;
    if (!check(TokenType::Semicolon) && !check(TokenType::RBrace)) {
        val = parse_expression();
    }
    return std::make_unique<ReturnExpr>(std::move(val), loc);
}

} // namespace oxide
