#include "lexer/lexer.h"
#include <sstream>

namespace chris {

const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"func",      TokenType::KwFunc},
    {"var",       TokenType::KwVar},
    {"let",       TokenType::KwLet},
    {"class",     TokenType::KwClass},
    {"interface", TokenType::KwInterface},
    {"enum",      TokenType::KwEnum},
    {"if",        TokenType::KwIf},
    {"else",      TokenType::KwElse},
    {"for",       TokenType::KwFor},
    {"while",     TokenType::KwWhile},
    {"return",    TokenType::KwReturn},
    {"import",    TokenType::KwImport},
    {"package",   TokenType::KwPackage},
    {"public",    TokenType::KwPublic},
    {"private",   TokenType::KwPrivate},
    {"protected", TokenType::KwProtected},
    {"throw",     TokenType::KwThrow},
    {"try",       TokenType::KwTry},
    {"catch",     TokenType::KwCatch},
    {"finally",   TokenType::KwFinally},
    {"async",     TokenType::KwAsync},
    {"await",     TokenType::KwAwait},
    {"io",        TokenType::KwIo},
    {"compute",   TokenType::KwCompute},
    {"unsafe",    TokenType::KwUnsafe},
    {"shared",    TokenType::KwShared},
    {"new",       TokenType::KwNew},
    {"match",     TokenType::KwMatch},
    {"operator",  TokenType::KwOperator},
    {"extern",    TokenType::KwExtern},
    {"in",        TokenType::KwIn},
    {"break",     TokenType::KwBreak},
    {"continue",  TokenType::KwContinue},
    {"true",      TokenType::KwTrue},
    {"false",     TokenType::KwFalse},
    {"this",      TokenType::KwThis},
    {"case",      TokenType::KwCase},
    {"nil",       TokenType::NilLiteral},
};

Lexer::Lexer(const std::string& source, const std::string& filename, DiagnosticEngine& diagnostics)
    : source_(source), filename_(filename), diagnostics_(diagnostics),
      pos_(0), line_(1), column_(1) {}

char Lexer::current() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::peek() const {
    return peekAt(1);
}

char Lexer::peekAt(size_t offset) const {
    size_t idx = pos_ + offset;
    if (idx >= source_.size()) return '\0';
    return source_[idx];
}

char Lexer::advance() {
    char c = current();
    pos_++;
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else {
            break;
        }
    }
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation(filename_, line_, column_);
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, currentLocation());
}

Token Lexer::makeToken(TokenType type) {
    return Token(type, "", currentLocation());
}

Token Lexer::lexNumber() {
    SourceLocation loc = currentLocation();
    std::string num;
    bool isFloat = false;

    while (!isAtEnd() && (std::isdigit(current()) || current() == '_')) {
        if (current() != '_') {
            num += current();
        }
        advance();
    }

    if (!isAtEnd() && current() == '.' && peek() != '.') {
        isFloat = true;
        num += current();
        advance();
        while (!isAtEnd() && (std::isdigit(current()) || current() == '_')) {
            if (current() != '_') {
                num += current();
            }
            advance();
        }
    }

    TokenType type = isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral;
    return Token(type, num, loc);
}

Token Lexer::lexString() {
    SourceLocation loc = currentLocation();
    advance(); // consume opening "

    std::string value;
    bool hasInterpolation = false;

    while (!isAtEnd() && current() != '"') {
        if (current() == '$' && peek() == '{') {
            hasInterpolation = true;
            break;
        }
        if (current() == '\\') {
            advance();
            if (isAtEnd()) {
                diagnostics_.error("E1001", "Unterminated string escape",
                                   currentLocation());
                return Token(TokenType::Error, value, loc);
            }
            switch (current()) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"':  value += '"'; break;
                case '$':  value += '$'; break;
                case '0':  value += '\0'; break;
                default:
                    diagnostics_.error("E1002",
                        std::string("Unknown escape sequence: \\") + current(),
                        currentLocation());
                    value += current();
                    break;
            }
            advance();
        } else {
            value += current();
            advance();
        }
    }

    if (!hasInterpolation) {
        if (isAtEnd()) {
            diagnostics_.error("E1003", "Unterminated string literal", loc);
            return Token(TokenType::Error, value, loc);
        }
        advance(); // consume closing "
        return Token(TokenType::StringLiteral, value, loc);
    }

    // String interpolation: return the first part as StringInterpStart
    return Token(TokenType::StringInterpStart, value, loc);
}

std::vector<Token> Lexer::lexStringInterpolation() {
    // Called when we've just returned a StringInterpStart.
    // We need to lex the expression inside ${...} and then continue the string.
    std::vector<Token> tokens;

    // Consume ${
    advance(); // $
    advance(); // {

    // Lex tokens inside the interpolation until we find matching }
    int braceDepth = 1;
    while (!isAtEnd() && braceDepth > 0) {
        skipWhitespace();
        if (isAtEnd()) break;

        if (current() == '}') {
            braceDepth--;
            if (braceDepth == 0) {
                advance(); // consume the closing }
                break;
            }
        }
        if (current() == '{') {
            braceDepth++;
        }

        // Lex one token inside the interpolation
        Token tok = lexSingleToken();
        if (tok.type != TokenType::EndOfFile) {
            tokens.push_back(tok);
        }
    }

    // Continue lexing the rest of the string
    SourceLocation loc = currentLocation();
    std::string value;
    bool moreInterpolation = false;

    while (!isAtEnd() && current() != '"') {
        if (current() == '$' && peek() == '{') {
            moreInterpolation = true;
            break;
        }
        if (current() == '\\') {
            advance();
            if (isAtEnd()) break;
            switch (current()) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"':  value += '"'; break;
                case '$':  value += '$'; break;
                case '0':  value += '\0'; break;
                default:   value += current(); break;
            }
            advance();
        } else {
            value += current();
            advance();
        }
    }

    if (moreInterpolation) {
        tokens.push_back(Token(TokenType::StringInterpMiddle, value, loc));
        auto more = lexStringInterpolation();
        tokens.insert(tokens.end(), more.begin(), more.end());
    } else {
        if (!isAtEnd()) advance(); // consume closing "
        tokens.push_back(Token(TokenType::StringInterpEnd, value, loc));
    }

    return tokens;
}

Token Lexer::lexChar() {
    SourceLocation loc = currentLocation();
    advance(); // consume opening '

    std::string value;
    if (isAtEnd()) {
        diagnostics_.error("E1004", "Unterminated character literal", loc);
        return Token(TokenType::Error, "", loc);
    }

    if (current() == '\\') {
        advance();
        if (isAtEnd()) {
            diagnostics_.error("E1004", "Unterminated character literal", loc);
            return Token(TokenType::Error, "", loc);
        }
        switch (current()) {
            case 'n':  value = "\n"; break;
            case 't':  value = "\t"; break;
            case 'r':  value = "\r"; break;
            case '\\': value = "\\"; break;
            case '\'': value = "'"; break;
            case '0':  value = std::string(1, '\0'); break;
            default:
                diagnostics_.error("E1002",
                    std::string("Unknown escape sequence: \\") + current(),
                    currentLocation());
                value = std::string(1, current());
                break;
        }
        advance();
    } else {
        value = std::string(1, current());
        advance();
    }

    if (isAtEnd() || current() != '\'') {
        diagnostics_.error("E1004", "Unterminated character literal", loc);
        return Token(TokenType::Error, value, loc);
    }
    advance(); // consume closing '

    return Token(TokenType::CharLiteral, value, loc);
}

Token Lexer::lexIdentifierOrKeyword() {
    SourceLocation loc = currentLocation();
    std::string ident;

    while (!isAtEnd() && (std::isalnum(current()) || current() == '_')) {
        ident += current();
        advance();
    }

    auto it = keywords_.find(ident);
    if (it != keywords_.end()) {
        TokenType kwType = it->second;
        if (kwType == TokenType::KwTrue || kwType == TokenType::KwFalse) {
            return Token(TokenType::BoolLiteral, ident, loc);
        }
        return Token(kwType, ident, loc);
    }

    return Token(TokenType::Identifier, ident, loc);
}

Token Lexer::lexLineComment() {
    SourceLocation loc = currentLocation();
    advance(); // first /
    advance(); // second /

    std::string value;
    while (!isAtEnd() && current() != '\n') {
        value += current();
        advance();
    }

    return Token(TokenType::LineComment, value, loc);
}

Token Lexer::lexDocComment() {
    SourceLocation loc = currentLocation();
    advance(); // first /
    advance(); // second /
    advance(); // third /

    std::string value;
    // Skip leading space
    if (!isAtEnd() && current() == ' ') {
        advance();
    }
    while (!isAtEnd() && current() != '\n') {
        value += current();
        advance();
    }

    return Token(TokenType::DocComment, value, loc);
}

Token Lexer::lexBlockComment() {
    SourceLocation loc = currentLocation();
    advance(); // /
    advance(); // *

    std::string value;
    int depth = 1;

    while (!isAtEnd() && depth > 0) {
        if (current() == '/' && peek() == '*') {
            depth++;
            value += current();
            advance();
            value += current();
            advance();
        } else if (current() == '*' && peek() == '/') {
            depth--;
            if (depth > 0) {
                value += current();
                advance();
                value += current();
                advance();
            } else {
                advance(); // *
                advance(); // /
            }
        } else {
            value += current();
            advance();
        }
    }

    if (depth > 0) {
        diagnostics_.error("E1005", "Unterminated block comment", loc);
        return Token(TokenType::Error, value, loc);
    }

    return Token(TokenType::BlockComment, value, loc);
}

Token Lexer::lexSingleToken() {
    skipWhitespace();
    if (isAtEnd()) return makeToken(TokenType::EndOfFile);

    SourceLocation loc = currentLocation();
    char c = current();

    // Numbers
    if (std::isdigit(c)) {
        return lexNumber();
    }

    // Strings
    if (c == '"') {
        return lexString();
    }

    // Characters
    if (c == '\'') {
        return lexChar();
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return lexIdentifierOrKeyword();
    }

    // Operators and punctuation
    advance();
    switch (c) {
        case '+':
            if (match('=')) return Token(TokenType::PlusAssign, "+=", loc);
            return Token(TokenType::Plus, "+", loc);
        case '*':
            if (match('=')) return Token(TokenType::StarAssign, "*=", loc);
            return Token(TokenType::Star, "*", loc);
        case '%':
            if (match('=')) return Token(TokenType::PercentAssign, "%=", loc);
            return Token(TokenType::Percent, "%", loc);
        case '(': return Token(TokenType::LeftParen, "(", loc);
        case ')': return Token(TokenType::RightParen, ")", loc);
        case '{': return Token(TokenType::LeftBrace, "{", loc);
        case '}': return Token(TokenType::RightBrace, "}", loc);
        case '[': return Token(TokenType::LeftBracket, "[", loc);
        case ']': return Token(TokenType::RightBracket, "]", loc);
        case ';': return Token(TokenType::Semicolon, ";", loc);
        case ':': return Token(TokenType::Colon, ":", loc);
        case ',': return Token(TokenType::Comma, ",", loc);
        case '@': return Token(TokenType::At, "@", loc);

        case '-':
            if (match('>')) return Token(TokenType::Arrow, "->", loc);
            if (match('=')) return Token(TokenType::MinusAssign, "-=", loc);
            return Token(TokenType::Minus, "-", loc);

        case '=':
            if (match('=')) return Token(TokenType::Equal, "==", loc);
            if (match('>')) return Token(TokenType::FatArrow, "=>", loc);
            return Token(TokenType::Assign, "=", loc);

        case '!':
            if (match('=')) return Token(TokenType::NotEqual, "!=", loc);
            return Token(TokenType::Not, "!", loc);

        case '<':
            if (match('=')) return Token(TokenType::LessEqual, "<=", loc);
            return Token(TokenType::Less, "<", loc);

        case '>':
            if (match('=')) return Token(TokenType::GreaterEqual, ">=", loc);
            return Token(TokenType::Greater, ">", loc);

        case '&':
            if (match('&')) return Token(TokenType::And, "&&", loc);
            diagnostics_.error("E1006", "Unexpected character '&'. Did you mean '&&'?", loc);
            return Token(TokenType::Error, "&", loc);

        case '|':
            if (match('|')) return Token(TokenType::Or, "||", loc);
            diagnostics_.error("E1006", "Unexpected character '|'. Did you mean '||'?", loc);
            return Token(TokenType::Error, "|", loc);

        case '.':
            if (match('.')) {
                if (match('.')) return Token(TokenType::Ellipsis, "...", loc);
                return Token(TokenType::DotDot, "..", loc);
            }
            return Token(TokenType::Dot, ".", loc);

        case '?':
            if (match('.')) return Token(TokenType::QuestionDot, "?.", loc);
            if (match('?')) return Token(TokenType::DoubleQuestion, "??", loc);
            return Token(TokenType::QuestionMark, "?", loc);

        case '/':
            if (match('=')) return Token(TokenType::SlashAssign, "/=", loc);
            if (current() == '/' && peek() == '/') {
                // Back up: we already consumed the first /
                // Actually we need to handle this differently
                // We consumed one /, check if next two are //
                return lexDocCommentFromSlash(loc);
            }
            if (current() == '/') {
                // Line comment: we consumed first /, consume second
                advance();
                std::string value;
                while (!isAtEnd() && current() != '\n') {
                    value += current();
                    advance();
                }
                return Token(TokenType::LineComment, value, loc);
            }
            if (current() == '*') {
                // Block comment: we consumed first /, now handle rest
                advance(); // consume *
                std::string value;
                int depth = 1;
                while (!isAtEnd() && depth > 0) {
                    if (current() == '/' && peek() == '*') {
                        depth++;
                        value += current(); advance();
                        value += current(); advance();
                    } else if (current() == '*' && peek() == '/') {
                        depth--;
                        if (depth > 0) {
                            value += current(); advance();
                            value += current(); advance();
                        } else {
                            advance(); advance();
                        }
                    } else {
                        value += current();
                        advance();
                    }
                }
                if (depth > 0) {
                    diagnostics_.error("E1005", "Unterminated block comment", loc);
                    return Token(TokenType::Error, value, loc);
                }
                return Token(TokenType::BlockComment, value, loc);
            }
            return Token(TokenType::Slash, "/", loc);

        default:
            diagnostics_.error("E1006",
                std::string("Unexpected character: '") + c + "'",
                loc);
            return Token(TokenType::Error, std::string(1, c), loc);
    }
}

Token Lexer::lexDocCommentFromSlash(const SourceLocation& loc) {
    // We already consumed the first '/'. Current char is '/'.
    advance(); // second /
    advance(); // third /

    std::string value;
    if (!isAtEnd() && current() == ' ') {
        advance();
    }
    while (!isAtEnd() && current() != '\n') {
        value += current();
        advance();
    }
    return Token(TokenType::DocComment, value, loc);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;

        Token tok = lexSingleToken();

        if (tok.type == TokenType::StringInterpStart) {
            tokens.push_back(tok);
            auto interpTokens = lexStringInterpolation();
            tokens.insert(tokens.end(), interpTokens.begin(), interpTokens.end());
            continue;
        }

        tokens.push_back(tok);
    }

    tokens.push_back(Token(TokenType::EndOfFile, "", currentLocation()));
    return tokens;
}

} // namespace chris
