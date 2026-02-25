#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "lexer/token.h"
#include "common/diagnostic.h"

namespace chris {

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename, DiagnosticEngine& diagnostics);

    std::vector<Token> tokenize();

private:
    char current() const;
    char peek() const;
    char peekAt(size_t offset) const;
    char advance();
    bool isAtEnd() const;
    bool match(char expected);

    void skipWhitespace();
    Token makeToken(TokenType type, const std::string& value);
    Token makeToken(TokenType type);
    SourceLocation currentLocation() const;

    Token lexNumber();
    Token lexString();
    Token lexChar();
    Token lexIdentifierOrKeyword();
    Token lexLineComment();
    Token lexBlockComment();
    Token lexDocComment();
    Token lexSingleToken();
    Token lexDocCommentFromSlash(const SourceLocation& loc);
    std::vector<Token> lexStringInterpolation();

    static const std::unordered_map<std::string, TokenType> keywords_;

    std::string source_;
    std::string filename_;
    DiagnosticEngine& diagnostics_;
    size_t pos_;
    uint32_t line_;
    uint32_t column_;
};

} // namespace chris
