#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "common/source_location.h"

namespace chris {

// Forward declarations
struct Expr;
struct Stmt;
struct TypeExpr;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using TypeExprPtr = std::unique_ptr<TypeExpr>;

// --- Type Expressions ---

struct TypeExpr {
    SourceLocation location;
    virtual ~TypeExpr() = default;
    virtual std::string toString() const = 0;
};

struct NamedType : TypeExpr {
    std::string name;
    bool nullable = false;
    std::vector<TypeExprPtr> typeArgs; // generic type arguments, e.g. Box<Int>

    std::string toString() const override {
        std::string result = name;
        if (!typeArgs.empty()) {
            result += "<";
            for (size_t i = 0; i < typeArgs.size(); i++) {
                if (i > 0) result += ", ";
                result += typeArgs[i]->toString();
            }
            result += ">";
        }
        if (nullable) result += "?";
        return result;
    }
};

// Access modifiers
enum class AccessModifier { Private, Protected, Public };

// --- Expressions ---

struct Expr {
    SourceLocation location;
    virtual ~Expr() = default;
    virtual std::string toString(int indent = 0) const = 0;
};

struct IntLiteralExpr : Expr {
    int64_t value;
    std::string toString(int indent = 0) const override;
};

struct FloatLiteralExpr : Expr {
    double value;
    std::string toString(int indent = 0) const override;
};

struct StringLiteralExpr : Expr {
    std::string value;
    std::string toString(int indent = 0) const override;
};

struct CharLiteralExpr : Expr {
    char value;
    std::string toString(int indent = 0) const override;
};

struct BoolLiteralExpr : Expr {
    bool value;
    std::string toString(int indent = 0) const override;
};

struct NilLiteralExpr : Expr {
    std::string toString(int indent = 0) const override;
};

struct IfExpr : Expr {
    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
    std::string toString(int indent = 0) const override;
};

struct IdentifierExpr : Expr {
    std::string name;
    std::string toString(int indent = 0) const override;
};

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left;
    ExprPtr right;
    std::string toString(int indent = 0) const override;
};

struct UnaryExpr : Expr {
    std::string op;
    ExprPtr operand;
    std::string toString(int indent = 0) const override;
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
    std::string toString(int indent = 0) const override;
};

struct MemberExpr : Expr {
    ExprPtr object;
    std::string member;
    std::string toString(int indent = 0) const override;
};

struct ThisExpr : Expr {
    std::string toString(int indent = 0) const override;
};

struct ConstructExpr : Expr {
    std::string className;
    std::vector<std::pair<std::string, ExprPtr>> fieldInits; // name: value pairs
    std::string toString(int indent = 0) const override;
};

struct AssignExpr : Expr {
    ExprPtr target;
    ExprPtr value;
    std::string toString(int indent = 0) const override;
};

struct RangeExpr : Expr {
    ExprPtr start;
    ExprPtr end;
    std::string toString(int indent = 0) const override;
};

// Nil coalescing: expr ?? defaultExpr
struct NilCoalesceExpr : Expr {
    ExprPtr value;
    ExprPtr defaultValue;
    std::string toString(int indent = 0) const override;
};

// Force unwrap: expr!
struct ForceUnwrapExpr : Expr {
    ExprPtr operand;
    std::string toString(int indent = 0) const override;
};

// Optional chain: expr?.member
struct OptionalChainExpr : Expr {
    ExprPtr object;
    std::string member;
    std::string toString(int indent = 0) const override;
};

struct StringInterpolationExpr : Expr {
    // Alternating string parts and expressions:
    // parts[0] + exprs[0] + parts[1] + exprs[1] + ... + parts[N]
    // parts.size() == exprs.size() + 1
    std::vector<std::string> parts;
    std::vector<ExprPtr> expressions;
    std::string toString(int indent = 0) const override;
};

// Array literal: [1, 2, 3]
struct ArrayLiteralExpr : Expr {
    std::vector<ExprPtr> elements;
    std::string toString(int indent = 0) const override;
};

// Index expression: arr[index]
struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    std::string toString(int indent = 0) const override;
};

// Lambda: (params) => expr  or  (params) => { block }
struct LambdaParam {
    std::string name;
    TypeExprPtr type; // optional type annotation
};

struct LambdaExpr : Expr {
    std::vector<LambdaParam> params;
    ExprPtr bodyExpr;                    // for single-expression lambdas
    std::unique_ptr<struct Block> bodyBlock; // for multi-line lambdas
    std::string toString(int indent = 0) const override;
};

// --- Statements ---

struct Stmt {
    SourceLocation location;
    virtual ~Stmt() = default;
    virtual std::string toString(int indent = 0) const = 0;
};

struct Block : Stmt {
    std::vector<StmtPtr> statements;
    std::string toString(int indent = 0) const override;
};

struct ExprStmt : Stmt {
    ExprPtr expression;
    std::string toString(int indent = 0) const override;
};

struct VarDecl : Stmt {
    std::string name;
    bool isMutable; // var = true, let = false
    TypeExprPtr typeAnnotation; // optional
    ExprPtr initializer;        // optional
    AccessModifier access = AccessModifier::Private; // default is private
    std::string toString(int indent = 0) const override;
};

struct ReturnStmt : Stmt {
    ExprPtr value; // optional
    std::string toString(int indent = 0) const override;
};

struct IfStmt : Stmt {
    ExprPtr condition;
    std::unique_ptr<Block> thenBlock;
    StmtPtr elseBlock; // either Block or another IfStmt (else if)
    std::string toString(int indent = 0) const override;
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    std::unique_ptr<Block> body;
    std::string toString(int indent = 0) const override;
};

struct ForStmt : Stmt {
    std::string variable;
    ExprPtr iterable;
    std::unique_ptr<Block> body;
    std::string toString(int indent = 0) const override;
};

struct BreakStmt : Stmt {
    std::string toString(int indent = 0) const override;
};

struct ContinueStmt : Stmt {
    std::string toString(int indent = 0) const override;
};

// Throw statement: throw expr;
struct ThrowStmt : Stmt {
    ExprPtr expression;
    std::string toString(int indent = 0) const override;
};

// Catch clause: catch (varName: TypeName) { block }
struct CatchClause {
    std::string varName;     // e.g. "e"
    std::string typeName;    // e.g. "DivisionError"
    std::unique_ptr<Block> body;
};

// Try/catch/finally: try { ... } catch (e: Error) { ... } finally { ... }
struct TryCatchStmt : Stmt {
    std::unique_ptr<Block> tryBlock;
    std::vector<CatchClause> catchClauses;
    std::unique_ptr<Block> finallyBlock; // optional
    std::string toString(int indent = 0) const override;
};

// Unsafe block: unsafe { ... }
struct UnsafeBlock : Stmt {
    std::unique_ptr<Block> body;
    std::string toString(int indent = 0) const override;
};

// --- Top-level Declarations ---

struct Parameter {
    std::string name;
    TypeExprPtr type;
    SourceLocation location;
};

// Async function kind: io or compute
enum class AsyncKind { None, Io, Compute };

struct FuncDecl : Stmt {
    std::string name;
    AccessModifier access = AccessModifier::Private; // default is private
    bool isOperator = false; // true for operator overloads (e.g. operator+)
    bool isAsync = false; // true for async functions
    AsyncKind asyncKind = AsyncKind::None; // io or compute annotation
    std::vector<Parameter> parameters;
    TypeExprPtr returnType; // optional (Void if absent)
    std::unique_ptr<Block> body;
    std::string toString(int indent = 0) const override;
};

struct ExternFuncDecl : Stmt {
    std::string name;
    std::vector<Parameter> parameters;
    TypeExprPtr returnType;
    bool isVariadic = false; // for C variadic functions like printf
    std::string toString(int indent = 0) const override;
};

struct ImportDecl : Stmt {
    std::string path; // e.g. "std.io"
    std::string toString(int indent = 0) const override;
};

struct ClassDecl : Stmt {
    std::string name;
    bool isPublic = false;
    bool isShared = false; // true for shared classes (thread-safe synchronized access)
    std::vector<std::string> typeParams; // generic type parameters, e.g. <T, U>
    std::string baseClass;  // empty if no inheritance
    std::vector<std::string> interfaces; // implemented interfaces
    std::vector<std::unique_ptr<VarDecl>> fields;
    std::vector<std::unique_ptr<FuncDecl>> methods;
    std::string toString(int indent = 0) const override;
};

struct InterfaceDecl : Stmt {
    std::string name;
    std::vector<std::unique_ptr<FuncDecl>> methods; // method signatures (no body)
    std::string toString(int indent = 0) const override;
};

// Enum variant: name + optional associated value type
struct EnumVariant {
    std::string name;
    TypeExprPtr associatedType; // nullptr for simple variants (e.g. Red), non-null for Ok(Int)
};

struct EnumDecl : Stmt {
    std::string name;
    std::vector<std::string> cases; // simple enum variant names (backward compat)
    std::vector<EnumVariant> variants; // extended variants with optional associated types
    std::string toString(int indent = 0) const override;
};

// Await expression: await expr
struct AwaitExpr : Expr {
    ExprPtr operand;
    std::string toString(int indent = 0) const override;
};

// Match arm: pattern => body
struct MatchArm {
    std::string enumName;  // e.g. "Color" (optional, for qualified patterns)
    std::string caseName;  // e.g. "Red" or "Ok"
    std::string bindingName; // e.g. "val" in Ok(val), empty if no binding
    StmtPtr body;          // the body to execute (Block or ExprStmt)
};

struct MatchExpr : Expr {
    ExprPtr subject;                    // the expression being matched
    std::vector<MatchArm> arms;
    std::string toString(int indent = 0) const override;
};

// Enum variant access: Color.Red
// Reuses MemberExpr for now — codegen distinguishes by checking classInfos_ vs enumInfos_

// --- Program (root node) ---

struct Program {
    std::vector<StmtPtr> declarations;
    std::string toString() const;
};

// Helper to create indentation string
std::string indentStr(int level);

} // namespace chris
