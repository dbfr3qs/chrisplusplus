#pragma once

#include <string>
#include "ast/ast.h"

namespace chris {

class Formatter {
public:
    Formatter(int indentWidth = 4);

    std::string format(const Program& program);

private:
    // Declarations
    std::string formatFuncDecl(const FuncDecl& func, int indent);
    std::string formatExternFuncDecl(const ExternFuncDecl& func, int indent);
    std::string formatVarDecl(const VarDecl& decl, int indent);
    std::string formatClassDecl(const ClassDecl& decl, int indent);
    std::string formatInterfaceDecl(const InterfaceDecl& decl, int indent);
    std::string formatEnumDecl(const EnumDecl& decl, int indent);
    std::string formatImportDecl(const ImportDecl& decl, int indent);

    // Statements
    std::string formatStmt(const Stmt& stmt, int indent);
    std::string formatBlock(const Block& block, int indent);
    std::string formatIfStmt(const IfStmt& stmt, int indent);
    std::string formatWhileStmt(const WhileStmt& stmt, int indent);
    std::string formatForStmt(const ForStmt& stmt, int indent);
    std::string formatReturnStmt(const ReturnStmt& stmt, int indent);
    std::string formatExprStmt(const ExprStmt& stmt, int indent);
    std::string formatThrowStmt(const ThrowStmt& stmt, int indent);
    std::string formatTryCatchStmt(const TryCatchStmt& stmt, int indent);
    std::string formatUnsafeBlock(const UnsafeBlock& stmt, int indent);

    // Expressions
    std::string formatExpr(const Expr& expr);
    std::string formatBinaryExpr(const BinaryExpr& expr);
    std::string formatUnaryExpr(const UnaryExpr& expr);
    std::string formatCallExpr(const CallExpr& expr);
    std::string formatMemberExpr(const MemberExpr& expr);
    std::string formatConstructExpr(const ConstructExpr& expr);
    std::string formatAssignExpr(const AssignExpr& expr);
    std::string formatLambdaExpr(const LambdaExpr& expr);
    std::string formatMatchExpr(const MatchExpr& expr);
    std::string formatStringInterpolation(const StringInterpolationExpr& expr);
    std::string formatArrayLiteral(const ArrayLiteralExpr& expr);
    std::string formatIndexExpr(const IndexExpr& expr);

    // Helpers
    std::string ind(int level) const;
    std::string formatParams(const std::vector<Parameter>& params);
    std::string formatTypeExpr(const TypeExpr& type);
    std::string formatAnnotations(const std::vector<Annotation>& annotations, int indent);

    int indentWidth_;
};

} // namespace chris
