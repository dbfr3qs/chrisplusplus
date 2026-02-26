#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"

#include "ast/ast.h"
#include "sema/types.h"
#include "common/diagnostic.h"

namespace chris {

class CodeGen {
public:
    CodeGen(const std::string& moduleName, DiagnosticEngine& diagnostics);

    bool generate(Program& program,
                  const std::vector<GenericInstantiation>& genericInstantiations = {});
    bool emitObjectFile(const std::string& outputPath);
    bool linkExecutable(const std::string& objectPath, const std::string& runtimePath,
                        const std::string& outputPath);
    std::string getIR() const;

    llvm::Module& module() { return *module_; }

private:
    // Declarations
    void emitFuncDecl(FuncDecl& func);
    void emitClassDecl(ClassDecl& decl);

    // Statements
    void emitStmt(Stmt& stmt);
    void emitBlock(Block& block);
    void emitVarDecl(VarDecl& decl);
    void emitIfStmt(IfStmt& stmt);
    void emitWhileStmt(WhileStmt& stmt);
    void emitForStmt(ForStmt& stmt);
    void emitReturnStmt(ReturnStmt& stmt);
    void emitExprStmt(ExprStmt& stmt);

    // Expressions — returns the LLVM Value
    llvm::Value* emitExpr(Expr& expr);
    llvm::Value* emitIntLiteral(IntLiteralExpr& expr);
    llvm::Value* emitFloatLiteral(FloatLiteralExpr& expr);
    llvm::Value* emitStringLiteral(StringLiteralExpr& expr);
    llvm::Value* emitBoolLiteral(BoolLiteralExpr& expr);
    llvm::Value* emitNilLiteral(NilLiteralExpr& expr);
    llvm::Value* emitIdentifier(IdentifierExpr& expr);
    llvm::Value* emitBinaryExpr(BinaryExpr& expr);
    llvm::Value* emitUnaryExpr(UnaryExpr& expr);
    llvm::Value* emitCallExpr(CallExpr& expr);
    llvm::Value* emitAssignExpr(AssignExpr& expr);
    llvm::Value* emitRangeExpr(RangeExpr& expr);
    llvm::Value* emitMemberExpr(MemberExpr& expr);
    llvm::Value* emitThisExpr(ThisExpr& expr);
    llvm::Value* emitConstructExpr(ConstructExpr& expr);
    llvm::Value* emitStringInterpolation(StringInterpolationExpr& expr);
    llvm::Value* emitNilCoalesceExpr(NilCoalesceExpr& expr);
    llvm::Value* emitForceUnwrapExpr(ForceUnwrapExpr& expr);
    llvm::Value* emitOptionalChainExpr(OptionalChainExpr& expr);
    llvm::Value* emitMatchExpr(MatchExpr& expr);
    llvm::Value* emitLambdaExpr(LambdaExpr& expr);
    llvm::Value* emitArrayLiteralExpr(ArrayLiteralExpr& expr);
    llvm::Value* emitIndexExpr(IndexExpr& expr);
    llvm::Value* emitIfExpr(IfExpr& expr);
    llvm::Value* emitAwaitExpr(AwaitExpr& expr);
    void emitEnumDecl(EnumDecl& decl);
    void emitThrowStmt(ThrowStmt& stmt);
    void emitTryCatchStmt(TryCatchStmt& stmt);

    // Helpers
    void declareRuntimeFunctions();
    llvm::Value* emitStringConcat(llvm::Value* left, llvm::Value* right);
    llvm::Value* emitIntToString(llvm::Value* val);
    llvm::Value* emitFloatToString(llvm::Value* val);
    llvm::Value* emitBoolToString(llvm::Value* val);
    llvm::Value* createGlobalString(const std::string& str);
    llvm::Type* getLLVMType(TypeExpr* typeExpr);
    llvm::Type* getLLVMTypeFromSema(const std::shared_ptr<Type>& type);
    llvm::Value* emitMemberStore(MemberExpr& member, llvm::Value* value);
    int getFieldIndex(const std::string& className, const std::string& fieldName);

    // Generics
    void emitGenericClassInstance(ClassDecl& templateDecl,
                                  const std::string& mangledName,
                                  const std::vector<std::string>& typeParams,
                                  const std::vector<std::shared_ptr<Type>>& typeArgs);

    // Variable storage (alloca-based)
    llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* func,
                                              const std::string& name,
                                              llvm::Type* type);

    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    DiagnosticEngine& diagnostics_;

    // Named values in current scope (variable name -> alloca)
    std::unordered_map<std::string, llvm::AllocaInst*> namedValues_;

    // Loop context for break/continue
    std::vector<llvm::BasicBlock*> breakTargets_;
    std::vector<llvm::BasicBlock*> continueTargets_;

    // Class support
    struct ClassInfo {
        llvm::StructType* structType;
        std::vector<std::string> fieldNames;
        std::vector<std::string> methodNames;
        std::string parentClass; // empty if no inheritance
        bool isShared = false; // true for shared classes (has mutex field at index 0)
    };
    std::unordered_map<std::string, ClassInfo> classInfos_;
    llvm::Value* thisPtr_ = nullptr; // current 'this' pointer in method
    std::string currentClassName_; // name of class being emitted (for member resolution)
    std::unordered_map<std::string, std::string> varClassMap_; // variable name -> class name

    // Enum support
    struct EnumInfo {
        std::string name;
        std::vector<std::string> cases;
        bool hasAssociatedValues = false; // true if any variant has associated data
        llvm::StructType* structType = nullptr; // {i64 tag, i64 value} for enums with associated values
        std::unordered_map<std::string, std::string> variantTypeNames; // case name -> type name (e.g. "Int", "String")
    };
    std::unordered_map<std::string, EnumInfo> enumInfos_;
    std::unordered_map<std::string, ClassDecl*> genericClassDecls_; // generic templates for instantiation
    int lambdaCounter_ = 0; // unique lambda name counter
    llvm::Type* lambdaParamTypeHint_ = nullptr; // hint for untyped lambda params (e.g. from array element type)

    // Runtime function declarations
    llvm::Function* runtimePrint_ = nullptr;
    llvm::Function* runtimeStrcat_ = nullptr;
    llvm::Function* runtimeIntToStr_ = nullptr;
    llvm::Function* runtimeFloatToStr_ = nullptr;
    llvm::Function* runtimeBoolToStr_ = nullptr;
    llvm::Function* runtimeCharToStr_ = nullptr;
    llvm::Function* printfFunc_ = nullptr;
    llvm::Function* runtimeTryBegin_ = nullptr;
    llvm::Function* runtimeTryEnd_ = nullptr;
    llvm::Function* runtimeThrow_ = nullptr;
    llvm::Function* runtimeGetException_ = nullptr;
    llvm::Function* runtimeGetJmpbuf_ = nullptr;
    llvm::Function* setjmpFunc_ = nullptr;
    llvm::Function* runtimeArrayAlloc_ = nullptr;
    llvm::Function* runtimeArrayBoundsCheck_ = nullptr;
    llvm::Function* runtimeStrToInt_ = nullptr;
    llvm::Function* runtimeStrToFloat_ = nullptr;
    llvm::Function* runtimeStrLen_ = nullptr;
    llvm::Function* runtimeStrContains_ = nullptr;
    llvm::Function* runtimeStrStartsWith_ = nullptr;
    llvm::Function* runtimeStrEndsWith_ = nullptr;
    llvm::Function* runtimeStrIndexOf_ = nullptr;
    llvm::Function* runtimeStrSubstring_ = nullptr;
    llvm::Function* runtimeStrReplace_ = nullptr;
    llvm::Function* runtimeStrTrim_ = nullptr;
    llvm::Function* runtimeStrToUpper_ = nullptr;
    llvm::Function* runtimeStrToLower_ = nullptr;
    llvm::Function* runtimeStrSplit_ = nullptr;
    llvm::Function* runtimeStrCharAt_ = nullptr;
    llvm::Function* runtimeArrayPush_ = nullptr;
    llvm::Function* runtimeArrayPop_ = nullptr;
    llvm::Function* runtimeArrayReverse_ = nullptr;
    llvm::Function* runtimeArrayJoin_ = nullptr;
    llvm::Function* runtimeArrayMap_ = nullptr;
    llvm::Function* runtimeArrayFilter_ = nullptr;
    llvm::Function* runtimeArrayForEach_ = nullptr;
    llvm::StructType* arrayStructType_ = nullptr; // {i64 length, ptr data}
    llvm::StructType* futureStructType_ = nullptr; // opaque Future* from runtime

    // Track variable -> array element type for indexing
    std::unordered_map<std::string, llvm::Type*> varArrayElemType_;

    // Async runtime functions
    llvm::Function* runtimeAsyncSpawn_ = nullptr;
    llvm::Function* runtimeAsyncAwait_ = nullptr;
    llvm::Function* runtimeAsyncRunLoop_ = nullptr;

    // Shared class mutex runtime functions
    llvm::Function* runtimeMutexInit_ = nullptr;
    llvm::Function* runtimeMutexLock_ = nullptr;
    llvm::Function* runtimeMutexUnlock_ = nullptr;
    llvm::Function* runtimeMutexDestroy_ = nullptr;

    // Channel runtime functions
    llvm::Function* runtimeChannelCreate_ = nullptr;
    llvm::Function* runtimeChannelSend_ = nullptr;
    llvm::Function* runtimeChannelRecv_ = nullptr;
    llvm::Function* runtimeChannelClose_ = nullptr;
    llvm::Function* runtimeChannelDestroy_ = nullptr;
};

} // namespace chris
