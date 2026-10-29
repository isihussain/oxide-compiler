#include "lexer.hpp"
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace oxide {

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::Eof: return "Eof";
        case TokenType::Invalid: return "Invalid";
        case TokenType::KwFn: return "fn";
        case TokenType::KwLet: return "let";
        case TokenType::KwMut: return "mut";
        case TokenType::KwIf: return "if";
        case TokenType::KwElse: return "else";
        case TokenType::KwLoop: return "loop";
        case TokenType::KwWhile: return "while";
        case TokenType::KwReturn: return "return";
        case TokenType::KwStruct: return "struct";
        case TokenType::KwEnum: return "enum";
        case TokenType::KwMatch: return "match";
        case TokenType::KwTrait: return "trait";
        case TokenType::KwImpl: return "impl";
        case TokenType::KwFor: return "for";
        case TokenType::KwSelfValue: return "self";
        case TokenType::KwExtern: return "extern";
        case TokenType::KwTrue: return "true";
        case TokenType::KwFalse: return "false";
        case TokenType::TypeI32: return "i32";
        case TokenType::TypeBool: return "bool";
        case TokenType::LiteralInt: return "Integer";
        case TokenType::LiteralBool: return "Boolean";
        case TokenType::LiteralString: return "String";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Underscore: return "_";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Dot: return ".";
        case TokenType::Eq: return "=";
        case TokenType::EqEq: return "==";
        case TokenType::NotEq: return "!=";
        case TokenType::Lt: return "<";
        case TokenType::LtEq: return "<=";
        case TokenType::Gt: return ">";
        case TokenType::GtEq: return ">=";
        case TokenType::Amp: return "&";
        case TokenType::Exclamation: return "!";
        case TokenType::Arrow: return "->";
        case TokenType::FatArrow: return "=>";
        case TokenType::Comma: return ",";
        case TokenType::Colon: return ":";
        case TokenType::ColonColon: return "::";
        case TokenType::Semicolon: return ";";
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::LBrace: return "{";
        case TokenType::RBrace: return "}";
    }
    return "Unknown";
}

std::string Token::to_string() const {
    std::ostringstream ss;
    ss << "[" << location.line << ":" << location.column << "] " << token_type_name(type);
    if (!text.empty()) {
        ss << "('" << text << "')";
    }
    return ss.str();
}

Lexer::Lexer(std::string source, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)) {}

bool Lexer::is_at_end() const {
    return cursor_ >= source_.size();
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[cursor_];
}

char Lexer::peek_next() const {
    if (cursor_ + 1 >= source_.size()) return '\0';
    return source_[cursor_ + 1];
}

char Lexer::advance() {
    char current = source_[cursor_++];
    if (current == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return current;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[cursor_] != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek_next() == '/') {
            // Single-line comment
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peek_next() == '*') {
            // Block comment
            advance(); // '/'
            advance(); // '*'
            while (!is_at_end()) {
                if (peek() == '*' && peek_next() == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;
        tokens.push_back(scan_token());
    }
    tokens.push_back(Token{TokenType::Eof, "", 0, false, {line_, column_}});
    return tokens;
}

Token Lexer::scan_token() {
    SourceLocation loc{line_, column_};
    char c = advance();

    if (std::isalpha(c) || c == '_') {
        cursor_--;
        if (column_ > 1) column_--;
        return scan_identifier_or_keyword();
    }

    if (std::isdigit(c)) {
        cursor_--;
        if (column_ > 1) column_--;
        return scan_number();
    }

    switch (c) {
        case '+': return Token{TokenType::Plus, "+", 0, false, loc};
        case '-': {
            if (match('>')) {
                return Token{TokenType::Arrow, "->", 0, false, loc};
            }
            return Token{TokenType::Minus, "-", 0, false, loc};
        }
        case '*': return Token{TokenType::Star, "*", 0, false, loc};
        case '/': return Token{TokenType::Slash, "/", 0, false, loc};
        case '.': return Token{TokenType::Dot, ".", 0, false, loc};
        case '=': {
            if (match('=')) {
                return Token{TokenType::EqEq, "==", 0, false, loc};
            }
            if (match('>')) {
                return Token{TokenType::FatArrow, "=>", 0, false, loc};
            }
            return Token{TokenType::Eq, "=", 0, false, loc};
        }
        case '!': {
            if (match('=')) {
                return Token{TokenType::NotEq, "!=", 0, false, loc};
            }
            return Token{TokenType::Exclamation, "!", 0, false, loc};
        }
        case '<': {
            if (match('=')) {
                return Token{TokenType::LtEq, "<=", 0, false, loc};
            }
            return Token{TokenType::Lt, "<", 0, false, loc};
        }
        case '>': {
            if (match('=')) {
                return Token{TokenType::GtEq, ">=", 0, false, loc};
            }
            return Token{TokenType::Gt, ">", 0, false, loc};
        }
        case '&': return Token{TokenType::Amp, "&", 0, false, loc};
        case ',': return Token{TokenType::Comma, ",", 0, false, loc};
        case ':': {
            if (match(':')) {
                return Token{TokenType::ColonColon, "::", 0, false, loc};
            }
            return Token{TokenType::Colon, ":", 0, false, loc};
        }
        case ';': return Token{TokenType::Semicolon, ";", 0, false, loc};
        case '(': return Token{TokenType::LParen, "(", 0, false, loc};
        case ')': return Token{TokenType::RParen, ")", 0, false, loc};
        case '{': return Token{TokenType::LBrace, "{", 0, false, loc};
        case '}': return Token{TokenType::RBrace, "}", 0, false, loc};
        case '"': return scan_string();
        default:
            return Token{TokenType::Invalid, std::string(1, c), 0, false, loc};
    }
}

Token Lexer::scan_identifier_or_keyword() {
    SourceLocation loc{line_, column_};
    size_t start = cursor_;
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }
    std::string text = source_.substr(start, cursor_ - start);

    if (text == "fn") return Token{TokenType::KwFn, text, 0, false, loc};
    if (text == "let") return Token{TokenType::KwLet, text, 0, false, loc};
    if (text == "mut") return Token{TokenType::KwMut, text, 0, false, loc};
    if (text == "if") return Token{TokenType::KwIf, text, 0, false, loc};
    if (text == "else") return Token{TokenType::KwElse, text, 0, false, loc};
    if (text == "loop") return Token{TokenType::KwLoop, text, 0, false, loc};
    if (text == "while") return Token{TokenType::KwWhile, text, 0, false, loc};
    if (text == "return") return Token{TokenType::KwReturn, text, 0, false, loc};
    if (text == "struct") return Token{TokenType::KwStruct, text, 0, false, loc};
    if (text == "enum") return Token{TokenType::KwEnum, text, 0, false, loc};
    if (text == "match") return Token{TokenType::KwMatch, text, 0, false, loc};
    if (text == "trait") return Token{TokenType::KwTrait, text, 0, false, loc};
    if (text == "impl") return Token{TokenType::KwImpl, text, 0, false, loc};
    if (text == "for") return Token{TokenType::KwFor, text, 0, false, loc};
    if (text == "self") return Token{TokenType::KwSelfValue, text, 0, false, loc};
    if (text == "extern") return Token{TokenType::KwExtern, text, 0, false, loc};
    if (text == "_") return Token{TokenType::Underscore, text, 0, false, loc};
    if (text == "true") return Token{TokenType::LiteralBool, text, 0, true, loc};
    if (text == "false") return Token{TokenType::LiteralBool, text, 0, false, loc};
    if (text == "i32") return Token{TokenType::TypeI32, text, 0, false, loc};
    if (text == "bool") return Token{TokenType::TypeBool, text, 0, false, loc};

    return Token{TokenType::Identifier, text, 0, false, loc};
}

Token Lexer::scan_number() {
    SourceLocation loc{line_, column_};
    size_t start = cursor_;
    while (!is_at_end() && std::isdigit(peek())) {
        advance();
    }
    std::string text = source_.substr(start, cursor_ - start);
    int64_t val = std::stoll(text);
    return Token{TokenType::LiteralInt, text, val, false, loc};
}

Token Lexer::scan_string() {
    SourceLocation loc{line_, column_};
    std::string val;
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (is_at_end()) break;
            char esc = advance();
            if (esc == 'n') val += '\n';
            else if (esc == 't') val += '\t';
            else if (esc == 'r') val += '\r';
            else if (esc == '\\') val += '\\';
            else if (esc == '"') val += '"';
            else val += esc;
        } else {
            val += advance();
        }
    }
    if (is_at_end()) {
        throw std::runtime_error("Unterminated string literal at [" +
                                 std::to_string(loc.line) + ":" + std::to_string(loc.column) + "]");
    }
    advance(); // consume closing '"'
    return Token{TokenType::LiteralString, val, 0, false, loc};
}

} // namespace oxide
