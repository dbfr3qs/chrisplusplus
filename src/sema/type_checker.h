#pragma once

#include "ast/ast.h"
#include "sema/types.h"
#include "sema/symbol_table.h"
#include "common/diagnostic.h"

namespace chris {

class TypeChecker {
public:
    TypeChecker(DiagnosticEngine& diagnostics);

    void check(Program& program);

    const std::vector<GenericInstantiation>& genericInstantiations() const {
        return genericInstantiations_;
    }

private:
    // Declarations
    void checkFuncDecl(FuncDecl& func);
    void checkVarDecl(VarDecl& decl);
    void checkImportDecl(ImportDecl& decl);
    void checkClassDecl(ClassDecl& decl);
    void checkInterfaceDecl(InterfaceDecl& decl);
    void checkEnumDecl(EnumDecl& decl);

    // Statements
    void checkStmt(Stmt& stmt);
    void checkBlock(Block& block);
    void checkIfStmt(IfStmt& stmt);
    void checkWhileStmt(WhileStmt& stmt);
    void checkForStmt(ForStmt& stmt);
    void checkReturnStmt(ReturnStmt& stmt);
    void checkExprStmt(ExprStmt& stmt);

    // Expressions — returns the inferred type
    TypePtr checkExpr(Expr& expr);
    TypePtr checkIntLiteral(IntLiteralExpr& expr);
    TypePtr checkFloatLiteral(FloatLiteralExpr& expr);
    TypePtr checkStringLiteral(StringLiteralExpr& expr);
    TypePtr checkBoolLiteral(BoolLiteralExpr& expr);
    TypePtr checkNilLiteral(NilLiteralExpr& expr);
    TypePtr checkIdentifier(IdentifierExpr& expr);
    TypePtr checkBinaryExpr(BinaryExpr& expr);
    TypePtr checkUnaryExpr(UnaryExpr& expr);
    TypePtr checkCallExpr(CallExpr& expr);
    TypePtr checkMemberExpr(MemberExpr& expr);
    TypePtr checkAssignExpr(AssignExpr& expr);
    TypePtr checkRangeExpr(RangeExpr& expr);
    TypePtr checkThisExpr(ThisExpr& expr);
    TypePtr checkConstructExpr(ConstructExpr& expr);
    TypePtr checkStringInterpolation(StringInterpolationExpr& expr);
    TypePtr checkNilCoalesceExpr(NilCoalesceExpr& expr);
    TypePtr checkForceUnwrapExpr(ForceUnwrapExpr& expr);
    TypePtr checkOptionalChainExpr(OptionalChainExpr& expr);
    TypePtr checkMatchExpr(MatchExpr& expr);
    TypePtr checkLambdaExpr(LambdaExpr& expr);
    TypePtr checkArrayLiteralExpr(ArrayLiteralExpr& expr);
    TypePtr checkIndexExpr(IndexExpr& expr);
    TypePtr checkIfExpr(IfExpr& expr);
    TypePtr checkAwaitExpr(AwaitExpr& expr);
    void checkThrowStmt(ThrowStmt& stmt);
    void checkTryCatchStmt(TryCatchStmt& stmt);
    void checkUnsafeBlock(UnsafeBlock& stmt);

    // Helpers
    TypePtr resolveTypeAnnotation(TypeExpr& typeExpr);
    void registerBuiltins();

    // Generics
    std::shared_ptr<ClassType> instantiateGenericClass(
        const std::string& name,
        const std::vector<TypePtr>& typeArgs);
    std::string mangledGenericName(const std::string& name, const std::vector<TypePtr>& typeArgs);

    DiagnosticEngine& diagnostics_;
    SymbolTable symbols_;
    TypePtr currentReturnType_; // expected return type of current function
    std::shared_ptr<ClassType> currentClass_; // class being checked (for 'this')
    std::unordered_map<std::string, std::shared_ptr<ClassType>> classTypes_; // registered classes
    std::unordered_map<std::string, std::shared_ptr<InterfaceType>> interfaceTypes_; // registered interfaces
    std::unordered_map<std::string, std::shared_ptr<EnumType>> enumTypes_; // registered enums
    std::unordered_map<std::string, ClassDecl*> genericClassDecls_; // generic class AST nodes for instantiation
    std::vector<std::string> currentTypeParams_; // type params in scope during generic class checking
    std::vector<GenericInstantiation> genericInstantiations_; // collected instantiations for codegen
    std::vector<TypePtr>* expectedLambdaParamTypes_ = nullptr; // propagated from call site for lambda inference
    bool inAsyncFunction_ = false; // true when checking inside an async function body
    bool inUnsafeBlock_ = false; // true when checking inside an unsafe block
};

} // namespace chris
