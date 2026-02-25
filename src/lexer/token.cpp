#include "lexer/token.h"
#include <sstream>

namespace chris {

bool Token::isKeyword() const {
    return type >= TokenType::KwFunc && type <= TokenType::KwFalse;
}

bool Token::isLiteral() const {
    return type >= TokenType::IntLiteral && type <= TokenType::NilLiteral;
}

bool Token::isOperator() const {
    return type >= TokenType::Plus && type <= TokenType::DoubleQuestion;
}

std::string Token::typeToString(TokenType type) {
    switch (type) {
        case TokenType::IntLiteral:        return "IntLiteral";
        case TokenType::FloatLiteral:      return "FloatLiteral";
        case TokenType::StringLiteral:     return "StringLiteral";
        case TokenType::StringInterpStart: return "StringInterpStart";
        case TokenType::StringInterpMiddle:return "StringInterpMiddle";
        case TokenType::StringInterpEnd:   return "StringInterpEnd";
        case TokenType::CharLiteral:       return "CharLiteral";
        case TokenType::BoolLiteral:       return "BoolLiteral";
        case TokenType::NilLiteral:        return "NilLiteral";
        case TokenType::Identifier:        return "Identifier";
        case TokenType::KwFunc:            return "func";
        case TokenType::KwVar:             return "var";
        case TokenType::KwLet:             return "let";
        case TokenType::KwClass:           return "class";
        case TokenType::KwInterface:       return "interface";
        case TokenType::KwEnum:            return "enum";
        case TokenType::KwIf:              return "if";
        case TokenType::KwElse:            return "else";
        case TokenType::KwFor:             return "for";
        case TokenType::KwWhile:           return "while";
        case TokenType::KwReturn:          return "return";
        case TokenType::KwImport:          return "import";
        case TokenType::KwPackage:         return "package";
        case TokenType::KwPublic:          return "public";
        case TokenType::KwPrivate:         return "private";
        case TokenType::KwProtected:       return "protected";
        case TokenType::KwThrow:           return "throw";
        case TokenType::KwTry:             return "try";
        case TokenType::KwCatch:           return "catch";
        case TokenType::KwFinally:         return "finally";
        case TokenType::KwAsync:           return "async";
        case TokenType::KwAwait:           return "await";
        case TokenType::KwIo:              return "io";
        case TokenType::KwCompute:         return "compute";
        case TokenType::KwUnsafe:          return "unsafe";
        case TokenType::KwShared:          return "shared";
        case TokenType::KwNew:             return "new";
        case TokenType::KwMatch:           return "match";
        case TokenType::KwOperator:        return "operator";
        case TokenType::KwExtern:          return "extern";
        case TokenType::KwIn:              return "in";
        case TokenType::KwBreak:           return "break";
        case TokenType::KwContinue:        return "continue";
        case TokenType::KwTrue:            return "true";
        case TokenType::KwFalse:           return "false";
        case TokenType::KwThis:            return "this";
        case TokenType::Plus:              return "+";
        case TokenType::Minus:             return "-";
        case TokenType::Star:              return "*";
        case TokenType::Slash:             return "/";
        case TokenType::Percent:           return "%";
        case TokenType::Assign:            return "=";
        case TokenType::Equal:             return "==";
        case TokenType::NotEqual:          return "!=";
        case TokenType::Less:              return "<";
        case TokenType::Greater:           return ">";
        case TokenType::LessEqual:         return "<=";
        case TokenType::GreaterEqual:      return ">=";
        case TokenType::And:               return "&&";
        case TokenType::Or:                return "||";
        case TokenType::Not:               return "!";
        case TokenType::Arrow:             return "->";
        case TokenType::FatArrow:          return "=>";
        case TokenType::DotDot:            return "..";
        case TokenType::QuestionMark:      return "?";
        case TokenType::QuestionDot:       return "?.";
        case TokenType::DoubleQuestion:    return "??";
        case TokenType::LeftParen:         return "(";
        case TokenType::RightParen:        return ")";
        case TokenType::LeftBrace:         return "{";
        case TokenType::RightBrace:        return "}";
        case TokenType::LeftBracket:       return "[";
        case TokenType::RightBracket:      return "]";
        case TokenType::Semicolon:         return ";";
        case TokenType::Colon:             return ":";
        case TokenType::Comma:             return ",";
        case TokenType::Dot:               return ".";
        case TokenType::At:                return "@";
        case TokenType::Ellipsis:          return "...";
        case TokenType::LineComment:       return "LineComment";
        case TokenType::BlockComment:      return "BlockComment";
        case TokenType::DocComment:        return "DocComment";
        case TokenType::EndOfFile:         return "EOF";
        case TokenType::Error:             return "Error";
    }
    return "Unknown";
}

std::string Token::toString() const {
    std::ostringstream oss;
    oss << typeToString(type);
    if (!value.empty()) {
        oss << "(" << value << ")";
    }
    oss << " at " << location.toString();
    return oss.str();
}

} // namespace chris
