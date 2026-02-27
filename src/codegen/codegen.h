#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
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
    std::unordered_set<std::string> varSetNames_; // variable names that hold Set<T>

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

    // Reflection runtime functions
    llvm::Function* runtimeTypeInfoName_ = nullptr;
    llvm::Function* runtimeTypeInfoFields_ = nullptr;
    llvm::Function* runtimeTypeInfoImplements_ = nullptr;
    llvm::StructType* typeInfoStructType_ = nullptr;
    std::unordered_map<std::string, llvm::GlobalVariable*> typeInfoGlobals_; // className -> global TypeInfo

    // Process execution runtime functions
    llvm::Function* runtimeExec_ = nullptr;
    llvm::Function* runtimeExecOutput_ = nullptr;

    // File I/O runtime functions
    llvm::Function* runtimeReadFile_ = nullptr;
    llvm::Function* runtimeWriteFile_ = nullptr;
    llvm::Function* runtimeAppendFile_ = nullptr;
    llvm::Function* runtimeFileExists_ = nullptr;

    // Map runtime functions
    llvm::Function* runtimeMapCreate_ = nullptr;
    llvm::Function* runtimeMapSet_ = nullptr;
    llvm::Function* runtimeMapGet_ = nullptr;
    llvm::Function* runtimeMapHas_ = nullptr;
    llvm::Function* runtimeMapDelete_ = nullptr;
    llvm::Function* runtimeMapSize_ = nullptr;
    llvm::Function* runtimeMapDestroy_ = nullptr;
    llvm::Function* runtimeMapKeys_ = nullptr;

    // Networking runtime functions (TCP)
    llvm::Function* runtimeTcpConnect_ = nullptr;
    llvm::Function* runtimeTcpListen_ = nullptr;
    llvm::Function* runtimeTcpAccept_ = nullptr;
    llvm::Function* runtimeTcpSend_ = nullptr;
    llvm::Function* runtimeTcpRecv_ = nullptr;
    llvm::Function* runtimeTcpClose_ = nullptr;

    // Networking runtime functions (UDP)
    llvm::Function* runtimeUdpCreate_ = nullptr;
    llvm::Function* runtimeUdpBind_ = nullptr;
    llvm::Function* runtimeUdpSendTo_ = nullptr;
    llvm::Function* runtimeUdpRecvFrom_ = nullptr;
    llvm::Function* runtimeUdpClose_ = nullptr;

    // DNS runtime function
    llvm::Function* runtimeDnsLookup_ = nullptr;

    // ConcurrentMap runtime functions
    llvm::Function* runtimeCmapCreate_ = nullptr;
    llvm::Function* runtimeCmapSet_ = nullptr;
    llvm::Function* runtimeCmapGet_ = nullptr;
    llvm::Function* runtimeCmapHas_ = nullptr;
    llvm::Function* runtimeCmapDelete_ = nullptr;
    llvm::Function* runtimeCmapSize_ = nullptr;
    llvm::Function* runtimeCmapDestroy_ = nullptr;

    // ConcurrentQueue runtime functions
    llvm::Function* runtimeCqueueCreate_ = nullptr;
    llvm::Function* runtimeCqueueEnqueue_ = nullptr;
    llvm::Function* runtimeCqueueDequeue_ = nullptr;
    llvm::Function* runtimeCqueueSize_ = nullptr;
    llvm::Function* runtimeCqueueIsEmpty_ = nullptr;
    llvm::Function* runtimeCqueueDestroy_ = nullptr;

    // Atomic runtime functions
    llvm::Function* runtimeAtomicCreate_ = nullptr;
    llvm::Function* runtimeAtomicLoad_ = nullptr;
    llvm::Function* runtimeAtomicStore_ = nullptr;
    llvm::Function* runtimeAtomicAdd_ = nullptr;
    llvm::Function* runtimeAtomicSub_ = nullptr;
    llvm::Function* runtimeAtomicCompareSwap_ = nullptr;
    llvm::Function* runtimeAtomicDestroy_ = nullptr;

    // HTTP runtime functions (client)
    llvm::Function* runtimeHttpGet_ = nullptr;
    llvm::Function* runtimeHttpPost_ = nullptr;

    // HTTP runtime functions (server)
    llvm::Function* runtimeHttpServerCreate_ = nullptr;
    llvm::Function* runtimeHttpServerAccept_ = nullptr;
    llvm::Function* runtimeHttpRequestMethod_ = nullptr;
    llvm::Function* runtimeHttpRequestPath_ = nullptr;
    llvm::Function* runtimeHttpRequestBody_ = nullptr;
    llvm::Function* runtimeHttpRespond_ = nullptr;
    llvm::Function* runtimeHttpServerClose_ = nullptr;

    // Set runtime functions
    llvm::Function* runtimeSetCreate_ = nullptr;
    llvm::Function* runtimeSetAdd_ = nullptr;
    llvm::Function* runtimeSetHas_ = nullptr;
    llvm::Function* runtimeSetRemove_ = nullptr;
    llvm::Function* runtimeSetSize_ = nullptr;
    llvm::Function* runtimeSetClear_ = nullptr;
    llvm::Function* runtimeSetValues_ = nullptr;
    llvm::Function* runtimeSetDestroy_ = nullptr;

    // Channel runtime functions
    llvm::Function* runtimeChannelCreate_ = nullptr;
    llvm::Function* runtimeChannelSend_ = nullptr;
    llvm::Function* runtimeChannelRecv_ = nullptr;
    llvm::Function* runtimeChannelClose_ = nullptr;
    llvm::Function* runtimeChannelDestroy_ = nullptr;

    // Math runtime functions (Int)
    llvm::Function* runtimeMathAbs_ = nullptr;
    llvm::Function* runtimeMathMin_ = nullptr;
    llvm::Function* runtimeMathMax_ = nullptr;
    llvm::Function* runtimeMathRandom_ = nullptr;

    // Math runtime functions (Float)
    llvm::Function* runtimeMathSqrt_ = nullptr;
    llvm::Function* runtimeMathPow_ = nullptr;
    llvm::Function* runtimeMathFloor_ = nullptr;
    llvm::Function* runtimeMathCeil_ = nullptr;
    llvm::Function* runtimeMathRound_ = nullptr;
    llvm::Function* runtimeMathLog_ = nullptr;
    llvm::Function* runtimeMathSin_ = nullptr;
    llvm::Function* runtimeMathCos_ = nullptr;
    llvm::Function* runtimeMathTan_ = nullptr;
    llvm::Function* runtimeMathFabs_ = nullptr;
    llvm::Function* runtimeMathFmin_ = nullptr;
    llvm::Function* runtimeMathFmax_ = nullptr;

    // Stdin runtime functions
    llvm::Function* runtimeReadLine_ = nullptr;

    // JSON runtime functions
    llvm::Function* runtimeJsonParse_ = nullptr;
    llvm::Function* runtimeJsonGet_ = nullptr;
    llvm::Function* runtimeJsonGetInt_ = nullptr;
    llvm::Function* runtimeJsonGetBool_ = nullptr;
    llvm::Function* runtimeJsonGetFloat_ = nullptr;
    llvm::Function* runtimeJsonGetArray_ = nullptr;
    llvm::Function* runtimeJsonGetObject_ = nullptr;
    llvm::Function* runtimeJsonArrayLength_ = nullptr;
    llvm::Function* runtimeJsonArrayGet_ = nullptr;
    llvm::Function* runtimeJsonStringify_ = nullptr;

    // Test framework runtime functions
    llvm::Function* runtimeAssert_ = nullptr;
    llvm::Function* runtimeAssertEqualInt_ = nullptr;
    llvm::Function* runtimeAssertEqualStr_ = nullptr;
    llvm::Function* runtimeTestSummary_ = nullptr;
    llvm::Function* runtimeTestExitCode_ = nullptr;

    // GC runtime functions
    llvm::Function* runtimeGcInit_ = nullptr;
    llvm::Function* runtimeGcAlloc_ = nullptr;
    llvm::Function* runtimeGcAllocFinalizer_ = nullptr;
    llvm::Function* runtimeGcSetNumPointers_ = nullptr;
    llvm::Function* runtimeGcCollect_ = nullptr;
    llvm::Function* runtimeGcShutdown_ = nullptr;
    llvm::Function* runtimeGcPushRoot_ = nullptr;
    llvm::Function* runtimeGcPopRoot_ = nullptr;
    llvm::Function* runtimeGcPopRoots_ = nullptr;

    // Track GC root count per function for pop_roots at return
    size_t currentFuncGcRootCount_ = 0;
};

} // namespace chris
