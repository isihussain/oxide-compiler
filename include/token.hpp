#pragma once

#include <string>
#include <string_view>

namespace oxide {

/**
 * @brief Discrete lexical token categories recognized by the Oxide scanner.
 *
 * Categorized into:
 *  - Stream bounds (Eof, Invalid)
 *  - Keywords (Control flow, types, traits, modifiers)
 *  - Scalar primitive types (i32, bool, str)
 *  - Literal values (integers, booleans, string slices)
 *  - Identifiers and wildcard '_'
 *  - Multi-character and single-character operators & delimiters
 */
enum class TokenType {
    // Stream termination & lexical error tokens
    Eof,
    Invalid,

    // Language keywords
    KwFn,          // 'fn' - function declaration
    KwLet,         // 'let' - variable binding
    KwMut,         // 'mut' - mutability qualifier
    KwIf,          // 'if' - conditional branch
    KwElse,        // 'else' - alternative branch
    KwLoop,        // 'loop' - infinite loop construct
    KwWhile,       // 'while' - conditional loop
    KwReturn,      // 'return' - explicit function return
    KwStruct,      // 'struct' - compound data type definition
    KwEnum,        // 'enum' - tagged union algebraic data type
    KwMatch,       // 'match' - pattern matching dispatch
    KwTrait,       // 'trait' - abstract behavior contract
    KwImpl,        // 'impl' - method or trait implementation block
    KwFor,         // 'for' - iteration loop
    KwIn,          // 'in' - membership / iterator delimiter
    KwSelfValue,   // 'self' - receiver value/reference in methods
    KwExtern,      // 'extern' - foreign function interface (FFI) block
    KwTrue,        // 'true' - boolean true literal
    KwFalse,       // 'false' - boolean false literal

    // Built-in primitive types
    TypeI32,       // 'i32' - 32-bit signed two's complement integer
    TypeF32,       // 'f32' - 32-bit single-precision IEEE 754 floating point
    TypeF64,       // 'f64' - 64-bit double-precision IEEE 754 floating point
    TypeBool,      // 'bool' - boolean flag
    TypeStr,       // 'str' - UTF-8 string slice

    // Literal value tokens
    LiteralInt,    // Decimal integer sequence (e.g., 42, 1000)
    LiteralFloat,  // Floating point sequence (e.g., 3.14159, 0.0)
    LiteralBool,   // Evaluated boolean constant
    LiteralString, // Double-quoted string literal (e.g., "Hello, world!\n")

    // Identifiers and pattern wildcards
    Identifier,    // Alphanumeric identifier [a-zA-Z_][a-zA-Z0-9_]*
    Underscore,    // '_' - catch-all wildcard pattern

    // Arithmetic, relational, and punctuation operators
    Plus,          // '+'
    Minus,         // '-'
    Star,          // '*'
    Slash,         // '/'
    Dot,           // '.'
    DotDot,        // '..' (range operator)
    Eq,            // '=' (assignment)
    EqEq,          // '==' (equality comparison)
    NotEq,         // '!=' (inequality comparison)
    Lt,            // '<' (less than or generic open bracket)
    LtEq,          // '<=' (less than or equal)
    Gt,            // '>' (greater than or generic close bracket)
    GtEq,          // '>=' (greater than or equal)
    Amp,           // '&' (borrow or reference operator)
    Exclamation,   // '!' (logical NOT or macro delimiter)
    Arrow,         // '->' (function return type delimiter)
    FatArrow,      // '=>' (match arm delimiter)
    Colon,         // ':' (type annotation delimiter)
    ColonColon,    // '::' (path separator for associated items and enum variants)
    Comma,         // ','
    Semicolon,     // ';'
    LParen,        // '('
    RParen,        // ')'
    LBrace,        // '{'
    RBrace,        // '}'
    LBracket,      // '['
    RBracket,      // ']'
    Hash,          // '#' (attribute prefix e.g. #[tmr])
};

/**
 * @brief Represents a 1-based source coordinate in a translation unit.
 *
 * Utilized by downstream compiler passes (parser, typeck, borrowck, codegen)
 * to emit precise diagnostics adhering to standard compiler error formats:
 *   oxide: error[line:column]: message
 */
struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
};

/**
 * @brief Discrete lexeme emitted by the scanner with metadata and evaluated literals.
 */
struct Token {
    TokenType type = TokenType::Invalid;
    std::string text;       ///< Exact string lexeme as scanned from source.
    int64_t int_value = 0;   ///< Pre-evaluated integer value for LiteralInt.
    bool bool_value = false; ///< Pre-evaluated boolean value for LiteralBool.
    SourceLocation location; ///< Location where the token starts.
    double float_value = 0.0; ///< Pre-evaluated floating point value for LiteralFloat.

    std::string to_string() const;
};

/**
 * @brief Converts a TokenType enum value into its human-readable string representation.
 */
const char* token_type_name(TokenType type);

} // namespace oxide
