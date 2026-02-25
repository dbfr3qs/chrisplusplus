#pragma once

#include <string>
#include "common/source_location.h"

namespace chris {

enum class TokenType {
    // Literals
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    StringInterpStart,    // "text${
    StringInterpMiddle,   // }text${
    StringInterpEnd,      // }text"
    CharLiteral,
    BoolLiteral,
    NilLiteral,

    // Identifier
    Identifier,

    // Keywords
    KwFunc,
    KwVar,
    KwLet,
    KwClass,
    KwInterface,
    KwEnum,
    KwIf,
    KwElse,
    KwFor,
    KwWhile,
    KwReturn,
    KwImport,
    KwPackage,
    KwPublic,
    KwPrivate,
    KwProtected,
    KwThrow,
    KwTry,
    KwCatch,
    KwFinally,
    KwAsync,
    KwAwait,
    KwIo,
    KwCompute,
    KwUnsafe,
    KwShared,
    KwNew,
    KwMatch,
    KwOperator,
    KwExtern,
    KwIn,
    KwBreak,
    KwContinue,
    KwTrue,
    KwFalse,
    KwThis,
    KwCase,

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    Assign,         // =
    Equal,          // ==
    NotEqual,       // !=
    Less,           // <
    Greater,        // >
    LessEqual,      // <=
    GreaterEqual,   // >=
    And,            // &&
    Or,             // ||
    Not,            // !
    Arrow,          // ->
    FatArrow,       // =>
    DotDot,         // ..
    QuestionMark,   // ?
    QuestionDot,    // ?.
    DoubleQuestion, // ??
    PlusAssign,     // +=
    MinusAssign,    // -=
    StarAssign,     // *=
    SlashAssign,    // /=
    PercentAssign,  // %=

    // Punctuation
    LeftParen,      // (
    RightParen,     // )
    LeftBrace,      // {
    RightBrace,     // }
    LeftBracket,    // [
    RightBracket,   // ]
    Semicolon,      // ;
    Colon,          // :
    Comma,          // ,
    Dot,            // .
    At,             // @
    Ellipsis,       // ...

    // Comments
    LineComment,    // //
    BlockComment,   // /* */
    DocComment,     // ///

    // Special
    EndOfFile,
    Error
};

struct Token {
    TokenType type;
    std::string value;
    SourceLocation location;

    Token() : type(TokenType::EndOfFile), value(""), location() {}
    Token(TokenType type, const std::string& value, const SourceLocation& location)
        : type(type), value(value), location(location) {}

    bool is(TokenType t) const { return type == t; }
    bool isKeyword() const;
    bool isLiteral() const;
    bool isOperator() const;

    static std::string typeToString(TokenType type);
    std::string toString() const;
};

} // namespace chris
