#include "sema/type_checker.h"
#include <sstream>
#include <set>

namespace chris {

TypeChecker::TypeChecker(DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics), currentReturnType_(nullptr) {
    registerBuiltins();
}

void TypeChecker::registerBuiltins() {
    // print(value: Any) -> Void — accepts any type, codegen handles conversion
    auto printType = makeFunctionType({unknownType()}, voidType());
    symbols_.define("print", printType, false, SourceLocation("<builtin>", 0, 0));

    // toString(value: Int) -> String  (implicit, for interpolation)
}

void TypeChecker::check(Program& program) {
    // Pass 0: register all interface and class names (so forward references work)
    for (auto& decl : program.declarations) {
        if (auto* iface = dynamic_cast<InterfaceDecl*>(decl.get())) {
            auto ifaceType = std::make_shared<InterfaceType>();
            ifaceType->name = iface->name;
            interfaceTypes_[iface->name] = ifaceType;
            symbols_.define(iface->name, ifaceType, false, iface->location);
        } else if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            auto classType = std::make_shared<ClassType>();
            classType->name = cls->name;
            classType->isShared = cls->isShared;
            classType->typeParams = cls->typeParams;
            classTypes_[cls->name] = classType;
            symbols_.define(cls->name, classType, false, cls->location);
            if (!cls->typeParams.empty()) {
                genericClassDecls_[cls->name] = cls;
            }
        } else if (auto* enm = dynamic_cast<EnumDecl*>(decl.get())) {
            auto enumType = std::make_shared<EnumType>();
            enumType->name = enm->name;
            enumType->cases = enm->cases;
            // Register associated value types from variants
            for (auto& variant : enm->variants) {
                if (variant.associatedType) {
                    enumType->associatedTypes[variant.name] = resolveTypeAnnotation(*variant.associatedType);
                }
            }
            enumTypes_[enm->name] = enumType;
            symbols_.define(enm->name, enumType, false, enm->location);
        }
    }

    // Pass 1: register all top-level function, class, and interface signatures
    for (auto& decl : program.declarations) {
        if (auto* func = dynamic_cast<FuncDecl*>(decl.get())) {
            std::vector<TypePtr> paramTypes;
            for (auto& param : func->parameters) {
                if (param.type) {
                    paramTypes.push_back(resolveTypeAnnotation(*param.type));
                } else {
                    paramTypes.push_back(unknownType());
                }
            }
            TypePtr retType = func->returnType
                ? resolveTypeAnnotation(*func->returnType)
                : voidType();
            // Async functions return Future<T> instead of T
            if (func->isAsync) {
                retType = makeFutureType(retType);
            }
            auto funcType = makeFunctionType(std::move(paramTypes), retType);
            if (!symbols_.define(func->name, funcType, false, func->location)) {
                diagnostics_.error("E3001",
                    "Function '" + func->name + "' is already defined",
                    func->location);
            }
        } else if (auto* ext = dynamic_cast<ExternFuncDecl*>(decl.get())) {
            std::vector<TypePtr> paramTypes;
            for (auto& param : ext->parameters) {
                if (param.type) {
                    paramTypes.push_back(resolveTypeAnnotation(*param.type));
                } else {
                    paramTypes.push_back(unknownType());
                }
            }
            TypePtr retType = ext->returnType
                ? resolveTypeAnnotation(*ext->returnType)
                : voidType();
            auto funcType = makeFunctionType(std::move(paramTypes), retType);
            if (!symbols_.define(ext->name, funcType, false, ext->location)) {
                diagnostics_.error("E3001",
                    "Function '" + ext->name + "' is already defined",
                    ext->location);
            }
        } else if (auto* iface = dynamic_cast<InterfaceDecl*>(decl.get())) {
            auto& ifaceType = interfaceTypes_[iface->name];
            for (auto& method : iface->methods) {
                std::vector<TypePtr> paramTypes;
                for (auto& param : method->parameters) {
                    paramTypes.push_back(param.type
                        ? resolveTypeAnnotation(*param.type)
                        : unknownType());
                }
                TypePtr retType = method->returnType
                    ? resolveTypeAnnotation(*method->returnType)
                    : voidType();
                auto methodType = makeFunctionType(std::move(paramTypes), retType);
                ifaceType->methods.push_back({method->name, methodType, true});
            }
        } else if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            auto& classType = classTypes_[cls->name];

            // Set type params scope for resolving types inside generic classes
            auto savedTypeParams = currentTypeParams_;
            currentTypeParams_ = cls->typeParams;

            // Set parent class
            if (!cls->baseClass.empty()) {
                auto parentIt = classTypes_.find(cls->baseClass);
                if (parentIt != classTypes_.end()) {
                    classType->parent = parentIt->second;
                } else {
                    // Could be an interface name used as first in the list
                    // Move it to interfaces list
                    cls->interfaces.insert(cls->interfaces.begin(), cls->baseClass);
                    cls->baseClass.clear();
                }
            }
            classType->interfaceNames = cls->interfaces;

            // Register fields
            for (auto& field : cls->fields) {
                TypePtr fieldType = field->typeAnnotation
                    ? resolveTypeAnnotation(*field->typeAnnotation)
                    : unknownType();
                AccessLevel al = AccessLevel::Private;
                if (field->access == AccessModifier::Public) al = AccessLevel::Public;
                else if (field->access == AccessModifier::Protected) al = AccessLevel::Protected;
                bool isPub = (al == AccessLevel::Public);
                classType->fields.push_back({field->name, fieldType, isPub, al});
            }

            // Register methods
            for (auto& method : cls->methods) {
                std::vector<TypePtr> paramTypes;
                for (auto& param : method->parameters) {
                    paramTypes.push_back(param.type
                        ? resolveTypeAnnotation(*param.type)
                        : unknownType());
                }
                TypePtr retType = method->returnType
                    ? resolveTypeAnnotation(*method->returnType)
                    : voidType();
                auto methodType = makeFunctionType(std::move(paramTypes), retType);
                AccessLevel al = AccessLevel::Private;
                if (method->access == AccessModifier::Public) al = AccessLevel::Public;
                else if (method->access == AccessModifier::Protected) al = AccessLevel::Protected;
                bool isPub = (al == AccessLevel::Public);
                classType->methods.push_back({method->name, methodType, isPub, al});
            }

            currentTypeParams_ = savedTypeParams;
        }
    }

    // Pass 2: check all declarations
    for (auto& decl : program.declarations) {
        checkStmt(*decl);
    }
}

// --- Declarations ---

void TypeChecker::checkFuncDecl(FuncDecl& func) {
    symbols_.pushScope();

    // Register parameters
    for (auto& param : func.parameters) {
        TypePtr paramType = param.type
            ? resolveTypeAnnotation(*param.type)
            : unknownType();
        if (!symbols_.define(param.name, paramType, false, param.location)) {
            diagnostics_.error("E3002",
                "Parameter '" + param.name + "' is already defined",
                param.location);
        }
    }

    // Set expected return type
    TypePtr prevReturnType = currentReturnType_;
    currentReturnType_ = func.returnType
        ? resolveTypeAnnotation(*func.returnType)
        : voidType();

    // Track async context
    bool prevAsync = inAsyncFunction_;
    inAsyncFunction_ = func.isAsync;

    // Validate io/compute annotation requires async
    if (func.asyncKind != AsyncKind::None && !func.isAsync) {
        diagnostics_.error("E3030",
            "io/compute annotation requires 'async' keyword on function '" + func.name + "'",
            func.location);
    }

    // Check body
    for (auto& stmt : func.body->statements) {
        checkStmt(*stmt);
    }

    inAsyncFunction_ = prevAsync;
    currentReturnType_ = prevReturnType;
    symbols_.popScope();
}

void TypeChecker::checkVarDecl(VarDecl& decl) {
    TypePtr declaredType = nullptr;
    if (decl.typeAnnotation) {
        declaredType = resolveTypeAnnotation(*decl.typeAnnotation);
    }

    TypePtr initType = nullptr;
    if (decl.initializer) {
        initType = checkExpr(*decl.initializer);
    }

    TypePtr varType = nullptr;
    if (declaredType && initType) {
        // Both declared and initialized — check compatibility
        if (!isAssignable(declaredType, initType)) {
            diagnostics_.error("E3003",
                "Cannot assign value of type '" + initType->toString() +
                "' to variable of type '" + declaredType->toString() + "'",
                decl.location);
        }
        varType = declaredType;
    } else if (declaredType) {
        varType = declaredType;
    } else if (initType) {
        // Type inference
        if (initType->kind() == TypeKind::Nil) {
            diagnostics_.error("E3004",
                "Cannot infer type from 'nil'. Add an explicit type annotation.",
                decl.location);
            varType = unknownType();
        } else {
            varType = initType;
        }
    } else {
        diagnostics_.error("E3005",
            "Variable '" + decl.name + "' must have a type annotation or initializer",
            decl.location);
        varType = unknownType();
    }

    if (!symbols_.define(decl.name, varType, decl.isMutable, decl.location)) {
        diagnostics_.error("E3006",
            "Variable '" + decl.name + "' is already defined in this scope",
            decl.location);
    }
}

void TypeChecker::checkImportDecl(ImportDecl& /*decl*/) {
    // Import resolution is not implemented yet — just accept it
}

void TypeChecker::checkInterfaceDecl(InterfaceDecl& /*decl*/) {
    // Interface methods are already registered in pass 1 — nothing more to check
}

void TypeChecker::checkEnumDecl(EnumDecl& /*decl*/) {
    // Enum cases are already registered in pass 0 — nothing more to check
}

void TypeChecker::checkClassDecl(ClassDecl& decl) {
    auto it = classTypes_.find(decl.name);
    if (it == classTypes_.end()) return;

    auto savedClass = currentClass_;
    currentClass_ = it->second;

    // Set type params scope for generic class body checking
    auto savedTypeParams = currentTypeParams_;
    currentTypeParams_ = decl.typeParams;

    // Check field initializers
    for (auto& field : decl.fields) {
        if (field->initializer) {
            checkExpr(*field->initializer);
        }
    }

    // Check methods
    for (auto& method : decl.methods) {
        checkFuncDecl(*method);
    }

    // Validate interface conformance
    auto& classType = it->second;
    for (auto& ifaceName : classType->interfaceNames) {
        auto ifaceIt = interfaceTypes_.find(ifaceName);
        if (ifaceIt == interfaceTypes_.end()) {
            diagnostics_.error("E3020",
                "Class '" + decl.name + "' implements unknown interface '" + ifaceName + "'",
                decl.location);
            continue;
        }
        auto& ifaceType = ifaceIt->second;
        for (auto& requiredMethod : ifaceType->methods) {
            bool found = false;
            for (auto& classMethod : classType->methods) {
                if (classMethod.name == requiredMethod.name) {
                    found = true;
                    // Check signature compatibility
                    if (!classMethod.type->equals(*requiredMethod.type)) {
                        diagnostics_.error("E3021",
                            "Method '" + requiredMethod.name + "' in class '" + decl.name +
                            "' does not match interface '" + ifaceName + "' signature",
                            decl.location);
                    }
                    break;
                }
            }
            if (!found) {
                diagnostics_.error("E3022",
                    "Class '" + decl.name + "' does not implement required method '" +
                    requiredMethod.name + "' from interface '" + ifaceName + "'",
                    decl.location);
            }
        }
    }

    currentTypeParams_ = savedTypeParams;
    currentClass_ = savedClass;
}

// --- Statements ---

void TypeChecker::checkStmt(Stmt& stmt) {
    if (auto* func = dynamic_cast<FuncDecl*>(&stmt)) {
        checkFuncDecl(*func);
    } else if (auto* var = dynamic_cast<VarDecl*>(&stmt)) {
        checkVarDecl(*var);
    } else if (auto* imp = dynamic_cast<ImportDecl*>(&stmt)) {
        checkImportDecl(*imp);
    } else if (auto* block = dynamic_cast<Block*>(&stmt)) {
        checkBlock(*block);
    } else if (auto* ifStmt = dynamic_cast<IfStmt*>(&stmt)) {
        checkIfStmt(*ifStmt);
    } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(&stmt)) {
        checkWhileStmt(*whileStmt);
    } else if (auto* forStmt = dynamic_cast<ForStmt*>(&stmt)) {
        checkForStmt(*forStmt);
    } else if (auto* retStmt = dynamic_cast<ReturnStmt*>(&stmt)) {
        checkReturnStmt(*retStmt);
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(&stmt)) {
        checkExprStmt(*exprStmt);
    } else if (auto* iface = dynamic_cast<InterfaceDecl*>(&stmt)) {
        checkInterfaceDecl(*iface);
    } else if (auto* cls = dynamic_cast<ClassDecl*>(&stmt)) {
        checkClassDecl(*cls);
    } else if (auto* enm = dynamic_cast<EnumDecl*>(&stmt)) {
        checkEnumDecl(*enm);
    } else if (dynamic_cast<BreakStmt*>(&stmt) || dynamic_cast<ContinueStmt*>(&stmt)) {
        // Valid, no type checking needed
    } else if (auto* throwStmt = dynamic_cast<ThrowStmt*>(&stmt)) {
        checkThrowStmt(*throwStmt);
    } else if (auto* tryCatch = dynamic_cast<TryCatchStmt*>(&stmt)) {
        checkTryCatchStmt(*tryCatch);
    } else if (auto* unsafeBlock = dynamic_cast<UnsafeBlock*>(&stmt)) {
        checkUnsafeBlock(*unsafeBlock);
    }
}

void TypeChecker::checkBlock(Block& block) {
    symbols_.pushScope();
    for (auto& stmt : block.statements) {
        checkStmt(*stmt);
    }
    symbols_.popScope();
}

void TypeChecker::checkIfStmt(IfStmt& stmt) {
    auto condType = checkExpr(*stmt.condition);
    if (condType && condType->kind() != TypeKind::Bool && condType->kind() != TypeKind::Unknown) {
        diagnostics_.error("E3007",
            "Condition must be of type 'Bool', got '" + condType->toString() + "'",
            stmt.condition->location);
    }

    symbols_.pushScope();
    for (auto& s : stmt.thenBlock->statements) {
        checkStmt(*s);
    }
    symbols_.popScope();

    if (stmt.elseBlock) {
        checkStmt(*stmt.elseBlock);
    }
}

void TypeChecker::checkWhileStmt(WhileStmt& stmt) {
    auto condType = checkExpr(*stmt.condition);
    if (condType && condType->kind() != TypeKind::Bool && condType->kind() != TypeKind::Unknown) {
        diagnostics_.error("E3007",
            "Condition must be of type 'Bool', got '" + condType->toString() + "'",
            stmt.condition->location);
    }

    symbols_.pushScope();
    for (auto& s : stmt.body->statements) {
        checkStmt(*s);
    }
    symbols_.popScope();
}

void TypeChecker::checkForStmt(ForStmt& stmt) {
    auto iterableType = checkExpr(*stmt.iterable);
    symbols_.pushScope();

    // Infer loop variable type from iterable
    TypePtr elemType = unknownType();
    if (auto* rangeExpr = dynamic_cast<RangeExpr*>(stmt.iterable.get())) {
        // Range of Int -> loop variable is Int
        (void)rangeExpr;
        elemType = intType();
    } else if (iterableType && iterableType->kind() == TypeKind::Array) {
        // Array iteration -> loop variable is the element type
        auto* arrType = static_cast<const ArrayType*>(iterableType.get());
        elemType = arrType->elementType;
    }

    symbols_.define(stmt.variable, elemType, false, stmt.location);
    for (auto& s : stmt.body->statements) {
        checkStmt(*s);
    }
    symbols_.popScope();
}

void TypeChecker::checkReturnStmt(ReturnStmt& stmt) {
    if (stmt.value) {
        // If returning a lambda and expected return type is a function, propagate param types
        if (auto* lambda = dynamic_cast<LambdaExpr*>(stmt.value.get())) {
            if (currentReturnType_ && currentReturnType_->kind() == TypeKind::Function) {
                auto& expectedFunc = static_cast<FunctionType&>(*currentReturnType_);
                expectedLambdaParamTypes_ = &expectedFunc.paramTypes;
            }
        }
        auto valueType = checkExpr(*stmt.value);
        expectedLambdaParamTypes_ = nullptr;
        // If current return type is unknown (lambda inference), infer it from the return value
        if (currentReturnType_ && currentReturnType_->kind() == TypeKind::Unknown && valueType) {
            currentReturnType_ = valueType;
        } else if (currentReturnType_ && valueType &&
            !isAssignable(currentReturnType_, valueType)) {
            diagnostics_.error("E3008",
                "Cannot return value of type '" + valueType->toString() +
                "' from function expecting '" + currentReturnType_->toString() + "'",
                stmt.location);
        }
    } else {
        if (currentReturnType_ && currentReturnType_->kind() != TypeKind::Void) {
            diagnostics_.error("E3008",
                "Non-void function must return a value of type '" +
                currentReturnType_->toString() + "'",
                stmt.location);
        }
    }
}

void TypeChecker::checkExprStmt(ExprStmt& stmt) {
    checkExpr(*stmt.expression);
}

// --- Expressions ---

TypePtr TypeChecker::checkExpr(Expr& expr) {
    if (auto* e = dynamic_cast<IntLiteralExpr*>(&expr))              return checkIntLiteral(*e);
    if (auto* e = dynamic_cast<FloatLiteralExpr*>(&expr))            return checkFloatLiteral(*e);
    if (auto* e = dynamic_cast<StringLiteralExpr*>(&expr))           return checkStringLiteral(*e);
    if (auto* e = dynamic_cast<CharLiteralExpr*>(&expr))             return charType();
    if (auto* e = dynamic_cast<BoolLiteralExpr*>(&expr))             return checkBoolLiteral(*e);
    if (auto* e = dynamic_cast<NilLiteralExpr*>(&expr))              return checkNilLiteral(*e);
    if (auto* e = dynamic_cast<IdentifierExpr*>(&expr))              return checkIdentifier(*e);
    if (auto* e = dynamic_cast<BinaryExpr*>(&expr))                  return checkBinaryExpr(*e);
    if (auto* e = dynamic_cast<UnaryExpr*>(&expr))                   return checkUnaryExpr(*e);
    if (auto* e = dynamic_cast<CallExpr*>(&expr))                    return checkCallExpr(*e);
    if (auto* e = dynamic_cast<MemberExpr*>(&expr))                  return checkMemberExpr(*e);
    if (auto* e = dynamic_cast<AssignExpr*>(&expr))                  return checkAssignExpr(*e);
    if (auto* e = dynamic_cast<RangeExpr*>(&expr))                   return checkRangeExpr(*e);
    if (auto* e = dynamic_cast<ThisExpr*>(&expr))                    return checkThisExpr(*e);
    if (auto* e = dynamic_cast<ConstructExpr*>(&expr))               return checkConstructExpr(*e);
    if (auto* e = dynamic_cast<StringInterpolationExpr*>(&expr))     return checkStringInterpolation(*e);
    if (auto* e = dynamic_cast<NilCoalesceExpr*>(&expr))            return checkNilCoalesceExpr(*e);
    if (auto* e = dynamic_cast<ForceUnwrapExpr*>(&expr))            return checkForceUnwrapExpr(*e);
    if (auto* e = dynamic_cast<OptionalChainExpr*>(&expr))          return checkOptionalChainExpr(*e);
    if (auto* e = dynamic_cast<MatchExpr*>(&expr))                  return checkMatchExpr(*e);
    if (auto* e = dynamic_cast<LambdaExpr*>(&expr))                 return checkLambdaExpr(*e);
    if (auto* e = dynamic_cast<ArrayLiteralExpr*>(&expr))           return checkArrayLiteralExpr(*e);
    if (auto* e = dynamic_cast<IndexExpr*>(&expr))                  return checkIndexExpr(*e);
    if (auto* e = dynamic_cast<IfExpr*>(&expr))                     return checkIfExpr(*e);
    if (auto* e = dynamic_cast<AwaitExpr*>(&expr))                   return checkAwaitExpr(*e);

    return unknownType();
}

TypePtr TypeChecker::checkIntLiteral(IntLiteralExpr& /*expr*/) {
    return intType();
}

TypePtr TypeChecker::checkFloatLiteral(FloatLiteralExpr& /*expr*/) {
    return floatType();
}

TypePtr TypeChecker::checkStringLiteral(StringLiteralExpr& /*expr*/) {
    return stringType();
}

TypePtr TypeChecker::checkBoolLiteral(BoolLiteralExpr& /*expr*/) {
    return boolType();
}

TypePtr TypeChecker::checkNilLiteral(NilLiteralExpr& /*expr*/) {
    return nilType();
}

TypePtr TypeChecker::checkIdentifier(IdentifierExpr& expr) {
    Symbol* sym = symbols_.lookup(expr.name);
    if (!sym) {
        diagnostics_.error("E3009",
            "Undefined variable '" + expr.name + "'",
            expr.location);
        return unknownType();
    }
    return sym->type;
}

TypePtr TypeChecker::checkBinaryExpr(BinaryExpr& expr) {
    auto leftType = checkExpr(*expr.left);
    auto rightType = checkExpr(*expr.right);

    if (!leftType || !rightType) return unknownType();
    if (leftType->kind() == TypeKind::Unknown || rightType->kind() == TypeKind::Unknown) {
        return unknownType();
    }

    // Check for operator overloading on class types
    if (leftType->kind() == TypeKind::Class) {
        auto* classType = dynamic_cast<const ClassType*>(leftType.get());
        if (classType) {
            std::string opMethodName = "operator" + expr.op;
            for (auto& method : classType->methods) {
                if (method.name == opMethodName) {
                    // Found operator overload — return its return type
                    auto* funcType = dynamic_cast<const FunctionType*>(method.type.get());
                    if (funcType) {
                        return funcType->returnType;
                    }
                }
            }
        }
    }

    // Arithmetic: +, -, *, /, %
    if (expr.op == "+" || expr.op == "-" || expr.op == "*" ||
        expr.op == "/" || expr.op == "%") {

        // String concatenation with +
        if (expr.op == "+" && leftType->kind() == TypeKind::String &&
            rightType->kind() == TypeKind::String) {
            return stringType();
        }

        if (!leftType->isNumeric() || !rightType->isNumeric()) {
            diagnostics_.error("E3010",
                "Operator '" + expr.op + "' requires numeric operands, got '" +
                leftType->toString() + "' and '" + rightType->toString() + "'",
                expr.location);
            return unknownType();
        }

        // If either is Float, result is Float
        if (leftType->kind() == TypeKind::Float || rightType->kind() == TypeKind::Float) {
            return floatType();
        }
        // If either is Float32 (and no Float64), result is Float32
        if (leftType->kind() == TypeKind::Float32 || rightType->kind() == TypeKind::Float32) {
            return float32Type();
        }
        // If either is Int (i64), result is Int
        if (leftType->kind() == TypeKind::Int || rightType->kind() == TypeKind::Int ||
            leftType->kind() == TypeKind::UInt || rightType->kind() == TypeKind::UInt) {
            return intType();
        }
        // Both are same sized type — return that type
        if (leftType->equals(*rightType)) {
            return leftType;
        }
        return intType();
    }

    // Comparison: <, >, <=, >=
    if (expr.op == "<" || expr.op == ">" || expr.op == "<=" || expr.op == ">=") {
        if (!leftType->isNumeric() || !rightType->isNumeric()) {
            diagnostics_.error("E3010",
                "Operator '" + expr.op + "' requires numeric operands, got '" +
                leftType->toString() + "' and '" + rightType->toString() + "'",
                expr.location);
        }
        return boolType();
    }

    // Equality: ==, !=
    if (expr.op == "==" || expr.op == "!=") {
        if (!leftType->equals(*rightType) &&
            !(leftType->isNumeric() && rightType->isNumeric())) {
            diagnostics_.error("E3010",
                "Operator '" + expr.op + "' requires matching types, got '" +
                leftType->toString() + "' and '" + rightType->toString() + "'",
                expr.location);
        }
        return boolType();
    }

    // Logical: &&, ||
    if (expr.op == "&&" || expr.op == "||") {
        if (leftType->kind() != TypeKind::Bool || rightType->kind() != TypeKind::Bool) {
            diagnostics_.error("E3010",
                "Operator '" + expr.op + "' requires Bool operands, got '" +
                leftType->toString() + "' and '" + rightType->toString() + "'",
                expr.location);
        }
        return boolType();
    }

    return unknownType();
}

TypePtr TypeChecker::checkUnaryExpr(UnaryExpr& expr) {
    auto operandType = checkExpr(*expr.operand);
    if (!operandType || operandType->kind() == TypeKind::Unknown) return unknownType();

    if (expr.op == "-") {
        if (!operandType->isNumeric()) {
            diagnostics_.error("E3011",
                "Unary '-' requires a numeric operand, got '" + operandType->toString() + "'",
                expr.location);
            return unknownType();
        }
        return operandType;
    }

    if (expr.op == "!") {
        if (operandType->kind() != TypeKind::Bool) {
            diagnostics_.error("E3011",
                "Unary '!' requires a Bool operand, got '" + operandType->toString() + "'",
                expr.location);
            return unknownType();
        }
        return boolType();
    }

    return unknownType();
}

TypePtr TypeChecker::checkCallExpr(CallExpr& expr) {
    auto calleeType = checkExpr(*expr.callee);

    if (!calleeType || calleeType->kind() == TypeKind::Unknown) {
        // Still check arguments
        for (auto& arg : expr.arguments) {
            checkExpr(*arg);
        }
        return unknownType();
    }

    if (calleeType->kind() != TypeKind::Function) {
        diagnostics_.error("E3012",
            "Expression is not callable (type: '" + calleeType->toString() + "')",
            expr.location);
        for (auto& arg : expr.arguments) {
            checkExpr(*arg);
        }
        return unknownType();
    }

    auto& funcType = static_cast<FunctionType&>(*calleeType);

    // Check arity
    if (expr.arguments.size() != funcType.paramTypes.size()) {
        diagnostics_.error("E3013",
            "Expected " + std::to_string(funcType.paramTypes.size()) +
            " argument(s), got " + std::to_string(expr.arguments.size()),
            expr.location);
    }

    // Check argument types — propagate expected types to lambda args for inference
    size_t count = std::min(expr.arguments.size(), funcType.paramTypes.size());
    for (size_t i = 0; i < count; i++) {
        // If the argument is a lambda and the expected type is a function, propagate param types
        if (auto* lambda = dynamic_cast<LambdaExpr*>(expr.arguments[i].get())) {
            auto expectedType = funcType.paramTypes[i];
            if (expectedType && expectedType->kind() == TypeKind::Function) {
                auto& expectedFunc = static_cast<FunctionType&>(*expectedType);
                expectedLambdaParamTypes_ = &expectedFunc.paramTypes;
            }
        }
        auto argType = checkExpr(*expr.arguments[i]);
        expectedLambdaParamTypes_ = nullptr;
        if (argType && !isAssignable(funcType.paramTypes[i], argType)) {
            diagnostics_.error("E3014",
                "Argument " + std::to_string(i + 1) + ": expected '" +
                funcType.paramTypes[i]->toString() + "', got '" +
                argType->toString() + "'",
                expr.arguments[i]->location);
        }
    }

    // Check remaining arguments if too many
    for (size_t i = count; i < expr.arguments.size(); i++) {
        checkExpr(*expr.arguments[i]);
    }

    return funcType.returnType;
}

TypePtr TypeChecker::checkMemberExpr(MemberExpr& expr) {
    auto objType = checkExpr(*expr.object);
    if (!objType || objType->kind() == TypeKind::Unknown) return unknownType();

    // Enum variant access: EnumName.CaseName or EnumName.CaseName(val)
    if (objType->kind() == TypeKind::Enum) {
        auto* enumType = static_cast<EnumType*>(objType.get());
        int idx = enumType->getCaseIndex(expr.member);
        if (idx >= 0) {
            // If variant has associated value, return a function type (T) -> Enum
            if (enumType->hasAssociatedValue(expr.member)) {
                auto assocType = enumType->associatedTypes.at(expr.member);
                return makeFunctionType({assocType}, objType);
            }
            return objType; // simple variant, returns the enum type
        }
    }

    // Array .length property and methods
    if (objType->kind() == TypeKind::Array) {
        auto* arrType = static_cast<ArrayType*>(objType.get());
        auto elemType = arrType->elementType;
        if (expr.member == "length") return intType();
        if (expr.member == "push") return makeFunctionType({elemType}, voidType());
        if (expr.member == "pop") return makeFunctionType({}, elemType);
        if (expr.member == "reverse") return makeFunctionType({}, objType);
        if (expr.member == "join") return makeFunctionType({stringType()}, stringType());
        if (expr.member == "map") {
            auto callbackType = makeFunctionType({elemType}, elemType);
            return makeFunctionType({callbackType}, objType);
        }
        if (expr.member == "filter") {
            auto callbackType = makeFunctionType({elemType}, boolType());
            return makeFunctionType({callbackType}, objType);
        }
        if (expr.member == "forEach") {
            auto callbackType = makeFunctionType({elemType}, voidType());
            return makeFunctionType({callbackType}, voidType());
        }
    }

    // Primitive type conversion methods
    if (objType->kind() == TypeKind::Int) {
        if (expr.member == "toString") return makeFunctionType({}, stringType());
        if (expr.member == "toFloat") return makeFunctionType({}, floatType());
        if (expr.member == "toChar") return makeFunctionType({}, charType());
    }
    if (objType->kind() == TypeKind::Int8 || objType->kind() == TypeKind::Int16 ||
        objType->kind() == TypeKind::Int32 || objType->kind() == TypeKind::UInt ||
        objType->kind() == TypeKind::UInt8 || objType->kind() == TypeKind::UInt16 ||
        objType->kind() == TypeKind::UInt32) {
        if (expr.member == "toString") return makeFunctionType({}, stringType());
        if (expr.member == "toInt") return makeFunctionType({}, intType());
        if (expr.member == "toFloat") return makeFunctionType({}, floatType());
    }
    if (objType->kind() == TypeKind::Char) {
        if (expr.member == "toString") return makeFunctionType({}, stringType());
        if (expr.member == "toInt") return makeFunctionType({}, intType());
    }
    if (objType->kind() == TypeKind::Float) {
        if (expr.member == "toString") return makeFunctionType({}, stringType());
        if (expr.member == "toInt") return makeFunctionType({}, intType());
    }
    if (objType->kind() == TypeKind::Float32) {
        if (expr.member == "toString") return makeFunctionType({}, stringType());
        if (expr.member == "toInt") return makeFunctionType({}, intType());
        if (expr.member == "toFloat") return makeFunctionType({}, floatType());
    }
    if (objType->kind() == TypeKind::Bool) {
        if (expr.member == "toString") return makeFunctionType({}, stringType());
    }
    if (objType->kind() == TypeKind::String) {
        if (expr.member == "toInt") return makeFunctionType({}, intType());
        if (expr.member == "toFloat") return makeFunctionType({}, floatType());
        if (expr.member == "length") return intType();
        if (expr.member == "contains") return makeFunctionType({stringType()}, boolType());
        if (expr.member == "startsWith") return makeFunctionType({stringType()}, boolType());
        if (expr.member == "endsWith") return makeFunctionType({stringType()}, boolType());
        if (expr.member == "indexOf") return makeFunctionType({stringType()}, intType());
        if (expr.member == "substring") return makeFunctionType({intType(), intType()}, stringType());
        if (expr.member == "replace") return makeFunctionType({stringType(), stringType()}, stringType());
        if (expr.member == "trim") return makeFunctionType({}, stringType());
        if (expr.member == "toUpper") return makeFunctionType({}, stringType());
        if (expr.member == "toLower") return makeFunctionType({}, stringType());
        if (expr.member == "split") return makeFunctionType({stringType()}, makeArrayType(stringType()));
        if (expr.member == "charAt") return makeFunctionType({intType()}, stringType());
    }

    if (objType->kind() == TypeKind::Class) {
        auto* classType = static_cast<ClassType*>(objType.get());

        // Check field access
        for (auto& field : classType->fields) {
            if (field.name == expr.member) {
                // Enforce access: if not inside this class, check access level
                if (currentClass_ && currentClass_->name == classType->name) {
                    // Inside the class — all access levels are OK
                } else if (currentClass_ && currentClass_->isSubclassOf(classType->name)) {
                    // Inside a subclass — protected and public OK
                    if (field.access == AccessLevel::Private) {
                        diagnostics_.error("E3023",
                            "Cannot access private field '" + expr.member + "' of class '" + classType->name + "'",
                            expr.location);
                    }
                } else {
                    // Outside the class — only public OK
                    if (field.access != AccessLevel::Public) {
                        diagnostics_.error("E3023",
                            "Cannot access " +
                            std::string(field.access == AccessLevel::Private ? "private" : "protected") +
                            " field '" + expr.member + "' of class '" + classType->name + "'",
                            expr.location);
                    }
                }
                return field.type;
            }
        }

        // Check method access
        for (auto& method : classType->methods) {
            if (method.name == expr.member) {
                if (currentClass_ && currentClass_->name == classType->name) {
                    // Inside the class — all access levels OK
                } else if (currentClass_ && currentClass_->isSubclassOf(classType->name)) {
                    if (method.access == AccessLevel::Private) {
                        diagnostics_.error("E3023",
                            "Cannot access private method '" + expr.member + "' of class '" + classType->name + "'",
                            expr.location);
                    }
                } else {
                    if (method.access != AccessLevel::Public) {
                        diagnostics_.error("E3023",
                            "Cannot access " +
                            std::string(method.access == AccessLevel::Private ? "private" : "protected") +
                            " method '" + expr.member + "' of class '" + classType->name + "'",
                            expr.location);
                    }
                }
                return method.type;
            }
        }

        // Check parent class
        auto fieldType = classType->getFieldType(expr.member);
        if (fieldType) return fieldType;
        auto methodType = classType->getMethodType(expr.member);
        if (methodType) return methodType;

        diagnostics_.error("E3018",
            "Class '" + classType->name + "' has no member '" + expr.member + "'",
            expr.location);
    }

    // Enum variant access: Color.Red
    if (objType->kind() == TypeKind::Enum) {
        auto* enumType = static_cast<EnumType*>(objType.get());
        if (enumType->getCaseIndex(expr.member) >= 0) {
            return objType; // The type of Color.Red is Color
        }
        diagnostics_.error("E3021",
            "Enum '" + enumType->name + "' has no case '" + expr.member + "'",
            expr.location);
    }

    return unknownType();
}

TypePtr TypeChecker::checkAssignExpr(AssignExpr& expr) {
    auto valueType = checkExpr(*expr.value);

    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.target.get())) {
        Symbol* sym = symbols_.lookup(ident->name);
        if (!sym) {
            diagnostics_.error("E3009",
                "Undefined variable '" + ident->name + "'",
                expr.target->location);
            return unknownType();
        }
        if (!sym->isMutable) {
            diagnostics_.error("E3015",
                "Cannot assign to immutable variable '" + ident->name + "' (declared with 'let')",
                expr.location);
        }
        if (valueType && !isAssignable(sym->type, valueType)) {
            diagnostics_.error("E3003",
                "Cannot assign value of type '" + valueType->toString() +
                "' to variable of type '" + sym->type->toString() + "'",
                expr.location);
        }
        return sym->type;
    }

    // For member assignments, just check the value
    checkExpr(*expr.target);
    return valueType ? valueType : unknownType();
}

TypePtr TypeChecker::checkRangeExpr(RangeExpr& expr) {
    auto startType = checkExpr(*expr.start);
    auto endType = checkExpr(*expr.end);

    if (startType && startType->kind() != TypeKind::Int && startType->kind() != TypeKind::Unknown) {
        diagnostics_.error("E3017",
            "Range start must be of type 'Int', got '" + startType->toString() + "'",
            expr.start->location);
    }
    if (endType && endType->kind() != TypeKind::Int && endType->kind() != TypeKind::Unknown) {
        diagnostics_.error("E3017",
            "Range end must be of type 'Int', got '" + endType->toString() + "'",
            expr.end->location);
    }

    return intType(); // Range<Int> — simplified for now
}

TypePtr TypeChecker::checkThisExpr(ThisExpr& expr) {
    if (!currentClass_) {
        diagnostics_.error("E3019",
            "'this' can only be used inside a class method",
            expr.location);
        return unknownType();
    }
    return currentClass_;
}

TypePtr TypeChecker::checkConstructExpr(ConstructExpr& expr) {
    auto it = classTypes_.find(expr.className);
    if (it == classTypes_.end()) {
        diagnostics_.error("E3020",
            "Unknown class '" + expr.className + "'",
            expr.location);
        return unknownType();
    }

    auto classType = it->second;

    // For generic template classes used inside their own methods,
    // the construct returns the template type (with type params unresolved)
    // Check that all provided fields exist and have correct types
    for (auto& [fieldName, fieldValue] : expr.fieldInits) {
        auto fieldType = classType->getFieldType(fieldName);
        if (!fieldType) {
            diagnostics_.error("E3021",
                "Class '" + expr.className + "' has no field '" + fieldName + "'",
                expr.location);
            checkExpr(*fieldValue);
            continue;
        }
        auto valueType = checkExpr(*fieldValue);
        // Skip type checking if field type is a type parameter (will be checked at instantiation)
        if (fieldType->kind() == TypeKind::TypeParameter) continue;
        if (valueType && fieldType && !isAssignable(fieldType, valueType)) {
            diagnostics_.error("E3003",
                "Cannot assign value of type '" + valueType->toString() +
                "' to field '" + fieldName + "' of type '" + fieldType->toString() + "'",
                expr.location);
        }
    }

    return classType;
}

TypePtr TypeChecker::checkStringInterpolation(StringInterpolationExpr& expr) {
    // All interpolated expressions are valid — they'll be converted to strings at runtime
    for (auto& e : expr.expressions) {
        checkExpr(*e);
    }
    return stringType();
}

TypePtr TypeChecker::resolveTypeAnnotation(TypeExpr& typeExpr) {
    if (auto* named = dynamic_cast<NamedType*>(&typeExpr)) {
        // Function type: __func convention from parser — typeArgs = [paramTypes..., returnType]
        if (named->name == "__func" && !named->typeArgs.empty()) {
            std::vector<TypePtr> paramTypes;
            for (size_t i = 0; i + 1 < named->typeArgs.size(); i++) {
                paramTypes.push_back(resolveTypeAnnotation(*named->typeArgs[i]));
            }
            auto returnType = resolveTypeAnnotation(*named->typeArgs.back());
            return makeFunctionType(std::move(paramTypes), returnType);
        }

        // Check if it's a type parameter in scope (e.g. T inside class Box<T>)
        for (auto& tp : currentTypeParams_) {
            if (tp == named->name) {
                auto result = makeTypeParameter(named->name);
                if (named->nullable) return makeNullable(result);
                return result;
            }
        }

        // Built-in Array<T> type
        if (named->name == "Array" && !named->typeArgs.empty()) {
            auto elemType = resolveTypeAnnotation(*named->typeArgs[0]);
            return makeArrayType(elemType);
        }

        // Built-in Future<T> type
        if (named->name == "Future" && !named->typeArgs.empty()) {
            auto innerType = resolveTypeAnnotation(*named->typeArgs[0]);
            return makeFutureType(innerType);
        }

        auto type = resolveTypeName(named->name);
        if (!type) {
            // Check if it's a class type
            auto it = classTypes_.find(named->name);
            if (it != classTypes_.end()) {
                type = it->second;
            } else if (auto eit = enumTypes_.find(named->name); eit != enumTypes_.end()) {
                type = eit->second;
            } else {
                diagnostics_.error("E3016",
                    "Unknown type '" + named->name + "'",
                    typeExpr.location);
                return unknownType();
            }
        }

        // Handle generic type arguments: Box<Int>, Pair<Int, String>
        if (!named->typeArgs.empty()) {
            if (type->kind() == TypeKind::Class) {
                auto* classType = static_cast<ClassType*>(type.get());
                if (classType->typeParams.size() != named->typeArgs.size()) {
                    diagnostics_.error("E3024",
                        "Generic class '" + named->name + "' expects " +
                        std::to_string(classType->typeParams.size()) +
                        " type argument(s), got " +
                        std::to_string(named->typeArgs.size()),
                        typeExpr.location);
                    return unknownType();
                }
                std::vector<TypePtr> resolvedArgs;
                for (auto& arg : named->typeArgs) {
                    resolvedArgs.push_back(resolveTypeAnnotation(*arg));
                }
                auto instantiated = instantiateGenericClass(named->name, resolvedArgs);
                if (named->nullable) return makeNullable(instantiated);
                return instantiated;
            }
        }

        if (named->nullable) {
            return makeNullable(type);
        }
        return type;
    }
    return unknownType();
}

TypePtr TypeChecker::checkNilCoalesceExpr(NilCoalesceExpr& expr) {
    TypePtr valueType = checkExpr(*expr.value);
    TypePtr defaultType = checkExpr(*expr.defaultValue);

    // The left side should be nullable
    if (valueType->isNullable()) {
        auto& nullable = static_cast<const NullableType&>(*valueType);
        // Result type is the unwrapped type (or the default type if compatible)
        if (!isAssignable(nullable.inner, defaultType)) {
            diagnostics_.error("E3020",
                "Nil coalescing default type '" + defaultType->toString() +
                "' is not compatible with '" + nullable.inner->toString() + "'",
                expr.location);
        }
        return nullable.inner;
    }

    // If left side is not nullable, just return its type (warning could be added)
    return valueType;
}

TypePtr TypeChecker::checkForceUnwrapExpr(ForceUnwrapExpr& expr) {
    TypePtr operandType = checkExpr(*expr.operand);

    if (operandType->isNullable()) {
        auto& nullable = static_cast<const NullableType&>(*operandType);
        return nullable.inner;
    }

    // Force unwrap on non-nullable is allowed but unnecessary
    return operandType;
}

TypePtr TypeChecker::checkOptionalChainExpr(OptionalChainExpr& expr) {
    TypePtr objType = checkExpr(*expr.object);

    // Unwrap nullable to get the inner type
    TypePtr innerType = objType;
    if (objType->isNullable()) {
        innerType = static_cast<const NullableType&>(*objType).inner;
    }

    // Look up member on the inner type (class type)
    auto* classType = dynamic_cast<const ClassType*>(innerType.get());
    if (classType) {
        TypePtr fieldType = classType->getFieldType(expr.member);
        if (fieldType) {
            // Result is always nullable (since the object might be nil)
            return makeNullable(fieldType);
        }
        TypePtr methodType = classType->getMethodType(expr.member);
        if (methodType) {
            return makeNullable(methodType);
        }
        diagnostics_.error("E3018",
            "No member '" + expr.member + "' on type '" + classType->name + "'",
            expr.location);
        return unknownType();
    }

    diagnostics_.error("E3019",
        "Cannot use optional chaining on non-class type '" + innerType->toString() + "'",
        expr.location);
    return unknownType();
}

TypePtr TypeChecker::checkMatchExpr(MatchExpr& expr) {
    TypePtr subjectType = checkExpr(*expr.subject);

    TypePtr resultType = nullptr;

    // Check each arm
    for (auto& arm : expr.arms) {
        // Validate the pattern against the subject type
        TypePtr bindingType = nullptr;
        if (subjectType->kind() == TypeKind::Enum) {
            auto* enumType = static_cast<EnumType*>(subjectType.get());
            if (enumType->getCaseIndex(arm.caseName) < 0) {
                diagnostics_.error("E3022",
                    "Enum '" + enumType->name + "' has no case '" + arm.caseName + "'",
                    expr.location);
            }
            // If arm has a binding and the variant has an associated type, resolve it
            if (!arm.bindingName.empty() && enumType->hasAssociatedValue(arm.caseName)) {
                bindingType = enumType->associatedTypes.at(arm.caseName);
            }
        }

        // Check arm body — if it's an ExprStmt, infer the arm's result type
        if (arm.body) {
            // Push scope for binding variable
            if (bindingType) {
                symbols_.pushScope();
                symbols_.define(arm.bindingName, bindingType, false, expr.location);
            }

            if (auto* exprStmt = dynamic_cast<ExprStmt*>(arm.body.get())) {
                auto armType = checkExpr(*exprStmt->expression);
                if (!resultType && armType && armType->kind() != TypeKind::Void) {
                    resultType = armType;
                }
            } else {
                checkStmt(*arm.body);
            }

            if (bindingType) {
                symbols_.popScope();
            }
        }
    }

    // Exhaustive match checking for enums
    if (subjectType && subjectType->kind() == TypeKind::Enum) {
        auto* enumType = static_cast<EnumType*>(subjectType.get());
        std::set<std::string> coveredCases;
        for (auto& arm : expr.arms) {
            coveredCases.insert(arm.caseName);
        }
        std::vector<std::string> missingCases;
        for (auto& caseName : enumType->cases) {
            if (coveredCases.find(caseName) == coveredCases.end()) {
                missingCases.push_back(caseName);
            }
        }
        if (!missingCases.empty()) {
            std::string missing;
            for (size_t i = 0; i < missingCases.size(); i++) {
                if (i > 0) missing += ", ";
                missing += missingCases[i];
            }
            diagnostics_.error("E3023",
                "Non-exhaustive match: missing case(s) " + missing +
                " for enum '" + enumType->name + "'",
                expr.location);
        }
    }

    return resultType ? resultType : voidType();
}

TypePtr TypeChecker::checkLambdaExpr(LambdaExpr& expr) {
    auto funcType = std::make_shared<FunctionType>();

    // Push scope for lambda parameters
    symbols_.pushScope();

    for (size_t i = 0; i < expr.params.size(); i++) {
        auto& param = expr.params[i];
        TypePtr paramType;
        if (param.type) {
            paramType = resolveTypeAnnotation(*param.type);
        } else if (expectedLambdaParamTypes_ && i < expectedLambdaParamTypes_->size()) {
            paramType = (*expectedLambdaParamTypes_)[i];
        } else {
            paramType = unknownType();
        }
        funcType->paramTypes.push_back(paramType);
        symbols_.define(param.name, paramType, false, expr.location);
    }

    // Check body
    if (expr.bodyExpr) {
        funcType->returnType = checkExpr(*expr.bodyExpr);
    } else if (expr.bodyBlock) {
        auto savedReturn = currentReturnType_;
        currentReturnType_ = unknownType();
        checkBlock(*expr.bodyBlock);
        funcType->returnType = currentReturnType_;
        currentReturnType_ = savedReturn;
        if (!funcType->returnType || funcType->returnType->kind() == TypeKind::Unknown) {
            funcType->returnType = voidType();
        }
    } else {
        funcType->returnType = voidType();
    }

    symbols_.popScope();
    return funcType;
}

TypePtr TypeChecker::checkIfExpr(IfExpr& expr) {
    auto condType = checkExpr(*expr.condition);
    if (condType && condType->kind() != TypeKind::Bool) {
        diagnostics_.error("E3010",
            "If expression condition must be Bool, got '" + condType->toString() + "'",
            expr.location);
    }
    auto thenType = checkExpr(*expr.thenExpr);
    auto elseType = checkExpr(*expr.elseExpr);
    // Return the then-branch type (both should match, but we trust the user)
    return thenType ? thenType : elseType;
}

TypePtr TypeChecker::checkAwaitExpr(AwaitExpr& expr) {
    if (!inAsyncFunction_) {
        diagnostics_.error("E3031",
            "'await' can only be used inside an async function",
            expr.location);
    }

    auto operandType = checkExpr(*expr.operand);
    if (!operandType) return unknownType();

    // If the operand is a Future<T>, unwrap to T
    if (operandType->kind() == TypeKind::Future) {
        auto& futureType = static_cast<const FutureType&>(*operandType);
        return futureType.innerType;
    }

    // Awaiting a non-future value just returns the value itself (implicit wrap/unwrap)
    return operandType;
}

TypePtr TypeChecker::checkArrayLiteralExpr(ArrayLiteralExpr& expr) {
    if (expr.elements.empty()) {
        return makeArrayType(unknownType());
    }
    TypePtr elemType = checkExpr(*expr.elements[0]);
    for (size_t i = 1; i < expr.elements.size(); i++) {
        auto t = checkExpr(*expr.elements[i]);
        if (elemType && t && !elemType->equals(*t) && elemType->kind() != TypeKind::Unknown) {
            diagnostics_.error("E3025",
                "Array element type mismatch: expected '" + elemType->toString() +
                "', got '" + t->toString() + "'",
                expr.elements[i]->location);
        }
    }
    return makeArrayType(elemType);
}

TypePtr TypeChecker::checkIndexExpr(IndexExpr& expr) {
    auto objType = checkExpr(*expr.object);
    auto idxType = checkExpr(*expr.index);

    if (!objType || objType->kind() == TypeKind::Unknown) return unknownType();

    if (objType->kind() == TypeKind::Array) {
        if (idxType && idxType->kind() != TypeKind::Int && idxType->kind() != TypeKind::Unknown) {
            diagnostics_.error("E3026",
                "Array index must be Int, got '" + idxType->toString() + "'",
                expr.index->location);
        }
        auto* arrType = static_cast<const ArrayType*>(objType.get());
        return arrType->elementType;
    }

    // String indexing: s[i] returns Char
    if (objType->kind() == TypeKind::String) {
        if (idxType && idxType->kind() != TypeKind::Int && idxType->kind() != TypeKind::Unknown) {
            diagnostics_.error("E3026",
                "String index must be Int, got '" + idxType->toString() + "'",
                expr.index->location);
        }
        return charType();
    }

    diagnostics_.error("E3027",
        "Type '" + objType->toString() + "' does not support indexing",
        expr.location);
    return unknownType();
}

void TypeChecker::checkThrowStmt(ThrowStmt& stmt) {
    // Check the thrown expression — any type is allowed for now
    checkExpr(*stmt.expression);
}

void TypeChecker::checkTryCatchStmt(TryCatchStmt& stmt) {
    // Check try block
    checkBlock(*stmt.tryBlock);

    // Check each catch clause
    for (auto& clause : stmt.catchClauses) {
        symbols_.pushScope();
        // Define the caught exception variable as String (message) for now
        symbols_.define(clause.varName, stringType(), false, stmt.location);
        checkBlock(*clause.body);
        symbols_.popScope();
    }

    // Check finally block
    if (stmt.finallyBlock) {
        checkBlock(*stmt.finallyBlock);
    }
}

void TypeChecker::checkUnsafeBlock(UnsafeBlock& stmt) {
    bool prevUnsafe = inUnsafeBlock_;
    inUnsafeBlock_ = true;
    checkBlock(*stmt.body);
    inUnsafeBlock_ = prevUnsafe;
}

std::string TypeChecker::mangledGenericName(const std::string& name, const std::vector<TypePtr>& typeArgs) {
    std::string result = name + "<";
    for (size_t i = 0; i < typeArgs.size(); i++) {
        if (i > 0) result += ", ";
        result += typeArgs[i]->toString();
    }
    result += ">";
    return result;
}

std::shared_ptr<ClassType> TypeChecker::instantiateGenericClass(
    const std::string& name,
    const std::vector<TypePtr>& typeArgs)
{
    // Check if already instantiated
    auto mangled = mangledGenericName(name, typeArgs);
    auto it = classTypes_.find(mangled);
    if (it != classTypes_.end()) {
        return it->second;
    }

    // Find the generic template
    auto templateIt = classTypes_.find(name);
    if (templateIt == classTypes_.end()) {
        return nullptr;
    }
    auto& templateType = templateIt->second;

    // Create instantiated class type
    auto instance = std::make_shared<ClassType>();
    instance->name = name;
    instance->typeParams = templateType->typeParams;
    instance->typeArgs = typeArgs;
    instance->parent = templateType->parent;
    instance->interfaceNames = templateType->interfaceNames;

    // Substitute type parameters in fields
    for (auto& field : templateType->fields) {
        ClassField newField;
        newField.name = field.name;
        newField.type = substituteTypeParams(field.type, templateType->typeParams, typeArgs);
        newField.isPublic = field.isPublic;
        newField.access = field.access;
        instance->fields.push_back(newField);
    }

    // Substitute type parameters in methods
    for (auto& method : templateType->methods) {
        ClassMethod newMethod;
        newMethod.name = method.name;
        newMethod.type = substituteTypeParams(method.type, templateType->typeParams, typeArgs);
        newMethod.isPublic = method.isPublic;
        newMethod.access = method.access;
        instance->methods.push_back(newMethod);
    }

    // Cache the instantiated type
    classTypes_[mangled] = instance;
    symbols_.define(mangled, instance, false, SourceLocation("<generic>", 0, 0));

    // Record for codegen
    genericInstantiations_.push_back({name, mangled, templateType->typeParams, typeArgs});

    return instance;
}

} // namespace chris
