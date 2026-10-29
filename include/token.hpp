#pragma once

#include <string>
#include <string_view>

namespace oxide {

enum class TokenType {
    // End of file / Invalid
    Eof,
    Invalid,

    // Keywords
    KwFn,
    KwLet,
    KwMut,
    KwIf,
    KwElse,
    KwLoop,
    KwWhile,
    KwReturn,
    KwStruct,
    KwEnum,
    KwMatch,
    KwTrait,
    KwImpl,
    KwFor,
    KwSelfValue,
    KwExtern,
    KwTrue,
    KwFalse,

    // Types
    TypeI32,
    TypeBool,

    // Literals
    LiteralInt,
    LiteralBool,
    LiteralString,

    // Identifiers
    Identifier,
    Underscore,    // _

    // Operators & Delimiters
    Plus,          // +
    Minus,         // -
    Star,          // *
    Slash,         // /
    Dot,           // .
    Eq,            // =
    EqEq,          // ==
    NotEq,         // !=
    Lt,            // <
    LtEq,          // <=
    Gt,            // >
    GtEq,          // >=
    Amp,           // &
    Exclamation,   // !
    Arrow,         // ->
    FatArrow,      // =>
    Colon,         // :
    ColonColon,    // ::
    Comma,         // ,
    Semicolon,     // ;
    LParen,        // (
    RParen,        // )
    LBrace,        // {
    RBrace,        // }
};

struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
};

struct Token {
    TokenType type = TokenType::Invalid;
    std::string text;
    int64_t int_value = 0;
    bool bool_value = false;
    SourceLocation location;

    std::string to_string() const;
};

const char* token_type_name(TokenType type);

} // namespace oxide
