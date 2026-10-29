#pragma once

#include "token.hpp"
#include <string>
#include <vector>

namespace oxide {

/**
 * @brief Lexical analyzer / scanner for the Oxide language.
 *
 * Implements a single-pass character-level streaming scanner. Features:
 *  - Accurate 1-based line and column tracking for source error locations.
 *  - Stripping of whitespace and comments (both single-line '//' and block comments).
 *  - Maximal munch rule for multi-character operators ('..', '->', '=>', '==', '!=', '<=', '>=', '::').
 *  - Escape sequence parsing within double-quoted string literals.
 *  - Numerical overflow-safe decimal integer literal evaluation.
 */
class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<stdin>");

    /**
     * @brief Transforms the entire input source string into a sequence of Tokens.
     * @return Vector of scanned tokens terminated by an explicit TokenType::Eof token.
     */
    std::vector<Token> tokenize();

private:
    char peek() const;
    char peek_next() const;
    char advance();
    bool match(char expected);
    bool is_at_end() const;

    void skip_whitespace_and_comments();
    Token scan_token();
    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_string();

    std::string source_;
    std::string filename_;
    size_t cursor_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
};

} // namespace oxide
