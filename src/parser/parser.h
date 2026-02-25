#pragma once

#include <vector>
#include <memory>
#include "lexer/token.h"
#include "ast/ast.h"
#include "common/diagnostic.h"

namespace chris {

class Parser {
public:
    Parser(const std::vector<Token>& tokens, DiagnosticEngine& diagnostics);

    Program parse();

private:
    // Token navigation
    const Token& current() const;
    const Token& peek() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& message);
    Token expectIdentifierOrKeyword(const std::string& message);
    bool isAtEnd() const;

    // Skip comments
    void skipComments();

    // Top-level
    StmtPtr parseDeclaration();
    StmtPtr parseImportDecl();
    std::unique_ptr<FuncDecl> parseFuncDecl();
    std::unique_ptr<FuncDecl> parseOperatorDecl();
    std::unique_ptr<ExternFuncDecl> parseExternFuncDecl();
    std::unique_ptr<ClassDecl> parseClassDecl(bool isPublic);
    std::unique_ptr<InterfaceDecl> parseInterfaceDecl();
    std::unique_ptr<EnumDecl> parseEnumDecl();
    StmtPtr parseStatement();

    // Statements
    std::unique_ptr<VarDecl> parseVarDecl();
    std::unique_ptr<IfStmt> parseIfStmt();
    std::unique_ptr<WhileStmt> parseWhileStmt();
    std::unique_ptr<ForStmt> parseForStmt();
    std::unique_ptr<ReturnStmt> parseReturnStmt();
    std::unique_ptr<BreakStmt> parseBreakStmt();
    std::unique_ptr<ContinueStmt> parseContinueStmt();
    std::unique_ptr<Block> parseBlock();
    std::unique_ptr<ExprStmt> parseExprStmt();
    std::unique_ptr<ThrowStmt> parseThrowStmt();
    std::unique_ptr<TryCatchStmt> parseTryCatchStmt();

    // Expressions (precedence climbing)
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseRange();
    ExprPtr parseNilCoalesce();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAddition();
    ExprPtr parseMultiplication();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();

    // Helpers
    ExprPtr parseStringInterpolation();
    TypeExprPtr parseTypeExpr();
    std::vector<ExprPtr> parseArguments();

    // Error recovery
    void synchronize();

    const std::vector<Token>& tokens_;
    DiagnosticEngine& diagnostics_;
    size_t pos_;
};

} // namespace chris
