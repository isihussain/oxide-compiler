#pragma once

#include "ast.hpp"
#include "token.hpp"
#include <string>
#include <unordered_set>
#include <vector>

namespace oxide {

/**
 * @brief Recursive-descent parser for the Oxide Rust compiler.
 *
 * Implements standard LL(k) recursive descent parsing combined with
 * operator precedence climbing (Pratt parsing) for expressions.
 *
 * Precedence hierarchy (lowest to highest):
 *   1. Assignment ('=') [Right-associative]
 *   2. Equality ('==', '!=') [Left-associative]
 *   3. Relational ('<', '<=', '>', '>=') [Left-associative]
 *   4. Additive / Term ('+', '-') [Left-associative]
 *   5. Multiplicative / Factor ('*', '/') [Left-associative]
 *   6. Prefix Unary ('-', '!', '&', '&mut', '*') [Right-associative]
 *   7. Postfix / Primary (field access '.', indexing '[]', function/method calls '()', literals, blocks)
 *
 * Disambiguation techniques:
 *  - Struct literal vs. block expression / comparison:
 *    Checks if an identifier is followed by '{' containing 'field:' syntax.
 *  - Generics vs. comparison operator '<':
 *    Scans ahead to distinguish 'foo<T>(...)' or 'struct Pair<T>' from 'x < y'.
 *  - Enum constructor vs. associated function:
 *    Resolves 'Type::Variant(args)' by maintaining a catalog of declared enums.
 */
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    /**
     * @brief Parses the entire token stream into a root Program AST node.
     * @return Unique pointer to the resulting Program.
     * @throws std::runtime_error on syntax errors with precise source coordinates.
     */
    std::unique_ptr<Program> parse_program();

private:
    const Token& peek() const;
    const Token& previous() const;
    bool is_at_end() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& error_message);

    std::unique_ptr<Function> parse_function(const std::string& target_type = "");
    std::unique_ptr<StructDef> parse_struct_definition();
    std::unique_ptr<EnumDef> parse_enum_definition();
    std::unique_ptr<TraitDef> parse_trait_definition();
    std::unique_ptr<ImplBlock> parse_impl_block();
    std::unique_ptr<ExternBlock> parse_extern_block();
    std::vector<std::string> parse_type_parameters();
    std::vector<Type> parse_type_arguments();
    Parameter parse_parameter(const std::string& target_type = "");
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
    std::unique_ptr<ForExpr> parse_for_expression();
    std::unique_ptr<ReturnExpr> parse_return_expression();
    std::unique_ptr<MatchExpr> parse_match_expression();

    std::vector<Token> tokens_;
    std::vector<std::string> current_type_params_;
    std::unordered_set<std::string> known_enums_;
    std::string current_target_type_;
    size_t cursor_ = 0;
};

} // namespace oxide
