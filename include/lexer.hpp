#pragma once

#include "token.hpp"
#include <string>
#include <vector>

namespace oxide {

class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<stdin>");

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
