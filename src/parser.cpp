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

    for (size_t i = 0; i < tokens_.size(); ++i) {
        if (tokens_[i].type == TokenType::KwEnum && i + 1 < tokens_.size() &&
            tokens_[i + 1].type == TokenType::Identifier) {
            known_enums_.insert(tokens_[i + 1].text);
        }
    }

    while (!is_at_end()) {
        if (check(TokenType::KwFn)) {
            program->functions.push_back(parse_function());
        } else if (check(TokenType::KwStruct)) {
            program->structs.push_back(parse_struct_definition());
        } else if (check(TokenType::KwEnum)) {
            program->enums.push_back(parse_enum_definition());
        } else if (check(TokenType::KwTrait)) {
            program->traits.push_back(parse_trait_definition());
        } else if (check(TokenType::KwImpl)) {
            program->impl_blocks.push_back(parse_impl_block());
        } else if (check(TokenType::KwExtern)) {
            if (cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].type == TokenType::LBrace) {
                program->extern_blocks.push_back(parse_extern_block());
            } else if (cursor_ + 2 < tokens_.size() &&
                       tokens_[cursor_ + 1].type == TokenType::LiteralString &&
                       tokens_[cursor_ + 2].type == TokenType::LBrace) {
                program->extern_blocks.push_back(parse_extern_block());
            } else if (cursor_ + 2 < tokens_.size() &&
                       tokens_[cursor_ + 1].type == TokenType::LiteralString &&
                       tokens_[cursor_ + 2].type == TokenType::KwFn) {
                advance(); // consume 'extern'
                std::string abi = advance().text; // consume literal string
                auto fn = parse_function();
                fn->abi = abi;
                program->functions.push_back(std::move(fn));
            } else {
                throw std::runtime_error("Unexpected token after 'extern' at [" +
                                         std::to_string(peek().location.line) + ":" +
                                         std::to_string(peek().location.column) + "]");
            }
        } else {
            throw std::runtime_error("Unexpected token at top level: " + peek().to_string());
        }
    }
    return program;
}

std::vector<std::string> Parser::parse_type_parameters() {
    std::vector<std::string> params;
    if (match(TokenType::Lt)) {
        if (!check(TokenType::Gt)) {
            do {
                params.push_back(consume(TokenType::Identifier, "Expected type parameter name").text);
            } while (match(TokenType::Comma));
        }
        consume(TokenType::Gt, "Expected '>' after type parameters");
    }
    return params;
}

std::vector<Type> Parser::parse_type_arguments() {
    std::vector<Type> args;
    if (match(TokenType::Lt)) {
        if (!check(TokenType::Gt)) {
            do {
                args.push_back(parse_type());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::Gt, "Expected '>' after type arguments");
    }
    return args;
}

std::unique_ptr<StructDef> Parser::parse_struct_definition() {
    SourceLocation loc = consume(TokenType::KwStruct, "Expected 'struct'").location;
    std::string name = consume(TokenType::Identifier, "Expected struct name").text;

    auto old_type_params = current_type_params_;
    auto type_params = parse_type_parameters();
    current_type_params_ = type_params;

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
    current_type_params_ = old_type_params;

    return std::make_unique<StructDef>(name, std::move(fields), loc, std::move(type_params));
}

std::unique_ptr<EnumDef> Parser::parse_enum_definition() {
    SourceLocation loc = consume(TokenType::KwEnum, "Expected 'enum'").location;
    std::string name = consume(TokenType::Identifier, "Expected enum name").text;

    consume(TokenType::LBrace, "Expected '{' after enum name");
    std::vector<EnumVariant> variants;
    int current_tag = 0;

    while (!check(TokenType::RBrace) && !is_at_end()) {
        const Token& var_tok = consume(TokenType::Identifier, "Expected variant name");
        std::optional<Type> payload_type;
        if (match(TokenType::LParen)) {
            payload_type = parse_type();
            consume(TokenType::RParen, "Expected ')' after payload type");
        }
        variants.push_back(EnumVariant{var_tok.text, payload_type, current_tag++, var_tok.location});
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    consume(TokenType::RBrace, "Expected '}' after enum variants");
    return std::make_unique<EnumDef>(name, std::move(variants), loc);
}

std::unique_ptr<TraitDef> Parser::parse_trait_definition() {
    SourceLocation loc = consume(TokenType::KwTrait, "Expected 'trait'").location;
    std::string name = consume(TokenType::Identifier, "Expected trait name").text;
    consume(TokenType::LBrace, "Expected '{' to start trait definition");

    std::vector<TraitMethod> methods;
    while (!check(TokenType::RBrace) && !is_at_end()) {
        SourceLocation m_loc = consume(TokenType::KwFn, "Expected 'fn' in trait definition").location;
        std::string m_name = consume(TokenType::Identifier, "Expected method name").text;

        consume(TokenType::LParen, "Expected '(' after method name");
        std::vector<Parameter> params;
        if (!check(TokenType::RParen)) {
            do {
                params.push_back(parse_parameter("Self"));
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RParen, "Expected ')' after parameters");

        Type return_type{BaseTypeKind::Unit, false, false};
        if (match(TokenType::Arrow)) {
            return_type = parse_type();
        }

        consume(TokenType::Semicolon, "Expected ';' after trait method prototype");
        methods.push_back(TraitMethod{m_name, std::move(params), return_type, m_loc});
    }
    consume(TokenType::RBrace, "Expected '}' to close trait definition");

    return std::make_unique<TraitDef>(name, std::move(methods), loc);
}

std::unique_ptr<ImplBlock> Parser::parse_impl_block() {
    SourceLocation loc = consume(TokenType::KwImpl, "Expected 'impl'").location;
    std::string first_id = consume(TokenType::Identifier, "Expected trait or type name after 'impl'").text;

    std::optional<std::string> trait_name = std::nullopt;
    std::string target_type;

    if (match(TokenType::KwFor)) {
        trait_name = first_id;
        target_type = consume(TokenType::Identifier, "Expected target type name after 'for'").text;
    } else {
        target_type = first_id;
    }

    consume(TokenType::LBrace, "Expected '{' to start impl block");

    std::vector<std::unique_ptr<Function>> methods;
    while (!check(TokenType::RBrace) && !is_at_end()) {
        auto fn = parse_function(target_type);
        methods.push_back(std::move(fn));
    }
    consume(TokenType::RBrace, "Expected '}' to close impl block");

    return std::make_unique<ImplBlock>(trait_name, target_type, std::move(methods), loc);
}

std::unique_ptr<ExternBlock> Parser::parse_extern_block() {
    SourceLocation loc = consume(TokenType::KwExtern, "Expected 'extern'").location;
    std::string abi = "C";
    if (check(TokenType::LiteralString)) {
        abi = advance().text;
    }
    consume(TokenType::LBrace, "Expected '{' to start extern block");
    std::vector<ForeignFunctionDecl> decls;
    while (!check(TokenType::RBrace) && !is_at_end()) {
        consume(TokenType::KwFn, "Expected 'fn' in extern block");
        const Token& name_tok = consume(TokenType::Identifier, "Expected foreign function name");
        consume(TokenType::LParen, "Expected '(' after function name");
        std::vector<Parameter> params;
        if (!check(TokenType::RParen)) {
            do {
                params.push_back(parse_parameter());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RParen, "Expected ')' after parameters");
        Type return_type{BaseTypeKind::Unit, "", false, false};
        if (match(TokenType::Arrow)) {
            return_type = parse_type();
        }
        consume(TokenType::Semicolon, "Expected ';' after foreign function declaration");
        decls.push_back(ForeignFunctionDecl{name_tok.text, std::move(params), return_type, name_tok.location});
    }
    consume(TokenType::RBrace, "Expected '}' to close extern block");
    return std::make_unique<ExternBlock>(abi, std::move(decls), loc);
}

std::unique_ptr<Function> Parser::parse_function(const std::string& target_type) {
    SourceLocation loc = consume(TokenType::KwFn, "Expected 'fn'").location;
    std::string raw_name = consume(TokenType::Identifier, "Expected function name").text;

    auto old_type_params = current_type_params_;
    auto old_target_type = current_target_type_;
    current_target_type_ = target_type;

    auto type_params = parse_type_parameters();
    current_type_params_ = type_params;

    consume(TokenType::LParen, "Expected '(' after function name");
    std::vector<Parameter> params;
    if (!check(TokenType::RParen)) {
        do {
            params.push_back(parse_parameter(target_type));
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')' after parameters");

    Type return_type{BaseTypeKind::Unit, false, false};
    if (match(TokenType::Arrow)) {
        return_type = parse_type();
    }

    auto body = parse_block();
    current_type_params_ = old_type_params;
    current_target_type_ = old_target_type;

    std::string fn_name = raw_name;
    std::string m_name = "";
    if (!target_type.empty()) {
        fn_name = target_type + "__" + raw_name;
        m_name = raw_name;
    }

    auto fn = std::make_unique<Function>(fn_name, std::move(params), return_type, std::move(body), loc, "", std::move(type_params));
    fn->method_name = m_name;
    fn->target_type = target_type;
    return fn;
}

Parameter Parser::parse_parameter(const std::string& target_type) {
    SourceLocation loc = peek().location;

    // &mut self
    if (match(TokenType::Amp)) {
        if (match(TokenType::KwMut)) {
            if (match(TokenType::KwSelfValue)) {
                std::string self_type_name = target_type.empty() ? "Self" : target_type;
                return Parameter{"self", Type{BaseTypeKind::Named, self_type_name, true, true}, true, loc};
            } else {
                throw std::runtime_error("Expected 'self' after '&mut' in parameter at [" +
                                         std::to_string(loc.line) + ":" + std::to_string(loc.column) + "]");
            }
        } else if (match(TokenType::KwSelfValue)) {
            std::string self_type_name = target_type.empty() ? "Self" : target_type;
            return Parameter{"self", Type{BaseTypeKind::Named, self_type_name, true, false}, false, loc};
        } else {
            throw std::runtime_error("Expected 'self' or type after '&' in parameter at [" +
                                     std::to_string(loc.line) + ":" + std::to_string(loc.column) + "]");
        }
    }

    // mut self or mut param: Type
    if (match(TokenType::KwMut)) {
        if (match(TokenType::KwSelfValue)) {
            if (match(TokenType::Colon)) {
                Type type = parse_type();
                return Parameter{"self", type, true, loc};
            }
            std::string self_type_name = target_type.empty() ? "Self" : target_type;
            return Parameter{"self", Type{BaseTypeKind::Named, self_type_name, false, false}, true, loc};
        }
        const Token& name_tok = consume(TokenType::Identifier, "Expected parameter name");
        consume(TokenType::Colon, "Expected ':' after parameter name");
        Type type = parse_type();
        return Parameter{name_tok.text, type, true, name_tok.location};
    }

    // self
    if (match(TokenType::KwSelfValue)) {
        if (match(TokenType::Colon)) {
            Type type = parse_type();
            return Parameter{"self", type, false, loc};
        }
        std::string self_type_name = target_type.empty() ? "Self" : target_type;
        return Parameter{"self", Type{BaseTypeKind::Named, self_type_name, false, false}, false, loc};
    }

    // Regular parameter: name: Type
    const Token& name_tok = consume(TokenType::Identifier, "Expected parameter name");
    consume(TokenType::Colon, "Expected ':' after parameter name");
    Type type = parse_type();
    return Parameter{name_tok.text, type, false, name_tok.location};
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
        std::string name = previous().text;
        if (name == "Self" && !current_target_type_.empty()) {
            name = current_target_type_;
        }
        bool is_param = false;
        for (const auto& tp : current_type_params_) {
            if (tp == name) {
                is_param = true;
                break;
            }
        }
        if (is_param) {
            type.kind = BaseTypeKind::GenericParam;
            type.name = name;
            return type;
        }

        type.kind = BaseTypeKind::Named;
        type.name = name;
        if (check(TokenType::Lt)) {
            type.type_args = parse_type_arguments();
        }
        return type;
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
        BinaryOp op = BinaryOp::Less;
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
    while (true) {
        if (match(TokenType::Dot)) {
            const Token& member_tok = consume(TokenType::Identifier, "Expected field or method name after '.'");
            std::vector<Type> type_args;
            if (check(TokenType::ColonColon) && cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].type == TokenType::Lt) {
                advance(); // consume '::'
                type_args = parse_type_arguments();
            }
            if (match(TokenType::LParen)) {
                std::vector<std::unique_ptr<Expression>> args;
                if (!check(TokenType::RParen)) {
                    do {
                        args.push_back(parse_expression());
                    } while (match(TokenType::Comma));
                }
                consume(TokenType::RParen, "Expected ')' after method arguments");
                expr = std::make_unique<MethodCallExpr>(std::move(expr), member_tok.text, std::move(args), member_tok.location, std::move(type_args));
            } else {
                expr = std::make_unique<FieldAccessExpr>(std::move(expr), member_tok.text, member_tok.location);
            }
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expression> Parser::parse_primary() {
    SourceLocation loc = peek().location;

    if (match(TokenType::KwSelfValue)) {
        return std::make_unique<VariableExpr>("self", previous().location);
    }

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

    if (match(TokenType::KwMatch)) {
        return parse_match_expression();
    }

    if (match(TokenType::Identifier)) {
        std::string name = previous().text;
        // Enum variant constructor or associated function: Name::Variant or Type::func(...)
        if (check(TokenType::ColonColon) && cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].type == TokenType::Identifier) {
            advance(); // consume '::'
            const Token& member_tok = consume(TokenType::Identifier, "Expected identifier after '::'");
            if (known_enums_.count(name)) {
                std::unique_ptr<Expression> payload = nullptr;
                if (match(TokenType::LParen)) {
                    payload = parse_expression();
                    consume(TokenType::RParen, "Expected ')' after variant payload");
                }
                return std::make_unique<EnumVariantExpr>(name, member_tok.text, std::move(payload), loc);
            } else {
                std::vector<Type> type_args;
                if (check(TokenType::ColonColon) && cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].type == TokenType::Lt) {
                    advance(); // consume '::'
                    type_args = parse_type_arguments();
                }
                if (match(TokenType::LParen)) {
                    std::vector<std::unique_ptr<Expression>> args;
                    if (!check(TokenType::RParen)) {
                        do {
                            args.push_back(parse_expression());
                        } while (match(TokenType::Comma));
                    }
                    consume(TokenType::RParen, "Expected ')' after call arguments");
                    return std::make_unique<CallExpr>(name + "::" + member_tok.text, std::move(args), loc, std::move(type_args));
                }
                return std::make_unique<VariableExpr>(name + "::" + member_tok.text, loc);
            }
        }

        // Turbofish type arguments: ident::<i32>
        std::vector<Type> type_args;
        if (check(TokenType::ColonColon) && cursor_ + 1 < tokens_.size() && tokens_[cursor_ + 1].type == TokenType::Lt) {
            advance(); // consume '::'
            type_args = parse_type_arguments();
        }

        // Function call: ident(...)
        if (match(TokenType::LParen)) {
            std::vector<std::unique_ptr<Expression>> args;
            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after call arguments");
            return std::make_unique<CallExpr>(name, std::move(args), loc, std::move(type_args));
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
            return std::make_unique<StructInitExpr>(name, std::move(inits), loc, std::move(type_args));
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

std::unique_ptr<MatchExpr> Parser::parse_match_expression() {
    SourceLocation loc = previous().location;
    auto target = parse_expression();
    consume(TokenType::LBrace, "Expected '{' after match target expression");

    std::vector<MatchArm> arms;
    while (!check(TokenType::RBrace) && !is_at_end()) {
        SourceLocation arm_loc = peek().location;
        MatchPattern pat;
        pat.location = arm_loc;

        if (match(TokenType::Underscore)) {
            pat.kind = PatternKind::Wildcard;
        } else {
            const Token& enum_tok = consume(TokenType::Identifier, "Expected enum name or '_' in match pattern");
            consume(TokenType::ColonColon, "Expected '::' after enum name in match pattern");
            const Token& var_tok = consume(TokenType::Identifier, "Expected variant name in match pattern");
            pat.kind = PatternKind::Variant;
            pat.enum_name = enum_tok.text;
            pat.variant_name = var_tok.text;
            if (match(TokenType::LParen)) {
                const Token& bind_tok = consume(TokenType::Identifier, "Expected variable name to bind variant payload");
                pat.binding_name = bind_tok.text;
                consume(TokenType::RParen, "Expected ')' after binding name");
            }
        }

        consume(TokenType::FatArrow, "Expected '=>' after match pattern");

        std::unique_ptr<Expression> body;
        if (check(TokenType::LBrace)) {
            body = parse_block();
            match(TokenType::Comma);
        } else {
            body = parse_expression();
            match(TokenType::Comma);
        }

        arms.push_back(MatchArm{pat, std::move(body), arm_loc});
    }

    consume(TokenType::RBrace, "Expected '}' to close match expression");
    return std::make_unique<MatchExpr>(std::move(target), std::move(arms), loc);
}

} // namespace oxide
