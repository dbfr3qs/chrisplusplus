#include "ast/ast.h"
#include <sstream>

namespace chris {

std::string indentStr(int level) {
    return std::string(level * 2, ' ');
}

// --- Expressions ---

std::string IntLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "(IntLiteral " + std::to_string(value) + ")";
}

std::string FloatLiteralExpr::toString(int indent) const {
    std::ostringstream oss;
    oss << indentStr(indent) << "(FloatLiteral " << value << ")";
    return oss.str();
}

std::string StringLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "(StringLiteral \"" + value + "\")";
}

std::string CharLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "(CharLiteral '" + std::string(1, value) + "')";
}

std::string BoolLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "(BoolLiteral " + (value ? "true" : "false") + ")";
}

std::string NilLiteralExpr::toString(int indent) const {
    return indentStr(indent) + "(NilLiteral)";
}

std::string IfExpr::toString(int indent) const {
    return indentStr(indent) + "(IfExpr " + condition->toString() +
           " then " + thenExpr->toString() +
           " else " + elseExpr->toString() + ")";
}

std::string IdentifierExpr::toString(int indent) const {
    return indentStr(indent) + "(Identifier " + name + ")";
}

std::string BinaryExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(BinaryExpr " + op + "\n";
    result += left->toString(indent + 1) + "\n";
    result += right->toString(indent + 1) + ")";
    return result;
}

std::string UnaryExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(UnaryExpr " + op + "\n";
    result += operand->toString(indent + 1) + ")";
    return result;
}

std::string CallExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(CallExpr\n";
    result += callee->toString(indent + 1) + "\n";
    result += indentStr(indent + 1) + "(Args";
    if (arguments.empty()) {
        result += ")";
    } else {
        result += "\n";
        for (size_t i = 0; i < arguments.size(); i++) {
            result += arguments[i]->toString(indent + 2);
            if (i + 1 < arguments.size()) result += "\n";
        }
        result += ")";
    }
    result += ")";
    return result;
}

std::string MemberExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(MemberExpr ." + member + "\n";
    result += object->toString(indent + 1) + ")";
    return result;
}

std::string ThisExpr::toString(int indent) const {
    return indentStr(indent) + "(This)";
}

std::string ConstructExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(Construct " + className;
    for (auto& [name, val] : fieldInits) {
        result += "\n" + indentStr(indent + 1) + name + ": " + val->toString(0);
    }
    result += ")";
    return result;
}

std::string AssignExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(AssignExpr\n";
    result += target->toString(indent + 1) + "\n";
    result += value->toString(indent + 1) + ")";
    return result;
}

std::string RangeExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(RangeExpr\n";
    result += start->toString(indent + 1) + "\n";
    result += end->toString(indent + 1) + ")";
    return result;
}

std::string LambdaExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(Lambda (";
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) result += ", ";
        result += params[i].name;
        if (params[i].type) result += ": " + params[i].type->toString();
    }
    result += ")";
    if (bodyExpr) {
        result += "\n" + bodyExpr->toString(indent + 1);
    } else if (bodyBlock) {
        result += "\n" + bodyBlock->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string NilCoalesceExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(NilCoalesce\n";
    result += value->toString(indent + 1) + "\n";
    result += defaultValue->toString(indent + 1) + ")";
    return result;
}

std::string ForceUnwrapExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(ForceUnwrap\n";
    result += operand->toString(indent + 1) + ")";
    return result;
}

std::string OptionalChainExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(OptionalChain\n";
    result += object->toString(indent + 1) + "\n";
    result += indentStr(indent + 1) + "(Member " + member + "))";
    return result;
}

std::string StringInterpolationExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(StringInterpolation";
    for (size_t i = 0; i < parts.size(); i++) {
        if (!parts[i].empty()) {
            result += "\n" + indentStr(indent + 1) + "(Part \"" + parts[i] + "\")";
        }
        if (i < expressions.size()) {
            result += "\n" + expressions[i]->toString(indent + 1);
        }
    }
    result += ")";
    return result;
}

std::string ArrayLiteralExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(ArrayLiteral";
    for (const auto& elem : elements) {
        result += "\n" + elem->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string IndexExpr::toString(int indent) const {
    return indentStr(indent) + "(Index\n" +
           object->toString(indent + 1) + "\n" +
           index->toString(indent + 1) + ")";
}

// --- Statements ---

std::string Block::toString(int indent) const {
    std::string result = indentStr(indent) + "(Block";
    for (const auto& stmt : statements) {
        result += "\n" + stmt->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string ExprStmt::toString(int indent) const {
    return indentStr(indent) + "(ExprStmt\n" + expression->toString(indent + 1) + ")";
}

std::string VarDecl::toString(int indent) const {
    std::string result = indentStr(indent) + "(" + (isMutable ? "VarDecl" : "LetDecl") + " " + name;
    if (typeAnnotation) {
        result += " : " + typeAnnotation->toString();
    }
    if (initializer) {
        result += "\n" + initializer->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string ReturnStmt::toString(int indent) const {
    std::string result = indentStr(indent) + "(ReturnStmt";
    if (value) {
        result += "\n" + value->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string IfStmt::toString(int indent) const {
    std::string result = indentStr(indent) + "(IfStmt\n";
    result += indentStr(indent + 1) + "(Condition\n" + condition->toString(indent + 2) + ")\n";
    result += indentStr(indent + 1) + "(Then\n" + thenBlock->toString(indent + 2) + ")";
    if (elseBlock) {
        result += "\n" + indentStr(indent + 1) + "(Else\n" + elseBlock->toString(indent + 2) + ")";
    }
    result += ")";
    return result;
}

std::string WhileStmt::toString(int indent) const {
    std::string result = indentStr(indent) + "(WhileStmt\n";
    result += indentStr(indent + 1) + "(Condition\n" + condition->toString(indent + 2) + ")\n";
    result += body->toString(indent + 1) + ")";
    return result;
}

std::string ForStmt::toString(int indent) const {
    std::string result = indentStr(indent) + "(ForStmt " + variable + " in\n";
    result += iterable->toString(indent + 1) + "\n";
    result += body->toString(indent + 1) + ")";
    return result;
}

std::string BreakStmt::toString(int indent) const {
    return indentStr(indent) + "(BreakStmt)";
}

std::string ContinueStmt::toString(int indent) const {
    return indentStr(indent) + "(ContinueStmt)";
}

std::string ThrowStmt::toString(int indent) const {
    std::string result = indentStr(indent) + "(ThrowStmt\n";
    result += expression->toString(indent + 1) + ")";
    return result;
}

std::string TryCatchStmt::toString(int indent) const {
    std::string result = indentStr(indent) + "(TryCatch\n";
    result += indentStr(indent + 1) + "(Try\n" + tryBlock->toString(indent + 2) + ")";
    for (auto& cc : catchClauses) {
        result += "\n" + indentStr(indent + 1) + "(Catch " + cc.varName + ": " + cc.typeName + "\n";
        result += cc.body->toString(indent + 2) + ")";
    }
    if (finallyBlock) {
        result += "\n" + indentStr(indent + 1) + "(Finally\n" + finallyBlock->toString(indent + 2) + ")";
    }
    result += ")";
    return result;
}

std::string UnsafeBlock::toString(int indent) const {
    std::string result = indentStr(indent) + "(Unsafe\n";
    result += body->toString(indent + 1) + ")";
    return result;
}

std::string AwaitExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(Await\n";
    result += operand->toString(indent + 1) + ")";
    return result;
}

std::string FuncDecl::toString(int indent) const {
    std::string result = indentStr(indent) + "(FuncDecl ";
    if (isAsync) {
        result += "async ";
        if (asyncKind == AsyncKind::Io) result += "io ";
        else if (asyncKind == AsyncKind::Compute) result += "compute ";
    }
    result += name;
    result += "\n" + indentStr(indent + 1) + "(Params";
    for (const auto& p : parameters) {
        result += "\n" + indentStr(indent + 2) + "(Param " + p.name;
        if (p.type) {
            result += " : " + p.type->toString();
        }
        result += ")";
    }
    result += ")";
    if (returnType) {
        result += "\n" + indentStr(indent + 1) + "(Returns " + returnType->toString() + ")";
    }
    result += "\n" + body->toString(indent + 1) + ")";
    return result;
}

std::string ExternFuncDecl::toString(int indent) const {
    std::string result = indentStr(indent) + "(ExternFuncDecl " + name;
    result += "\n" + indentStr(indent + 1) + "(Params";
    for (const auto& p : parameters) {
        result += "\n" + indentStr(indent + 2) + "(Param " + p.name;
        if (p.type) {
            result += " : " + p.type->toString();
        }
        result += ")";
    }
    if (isVariadic) result += "\n" + indentStr(indent + 2) + "(Variadic)";
    result += ")";
    if (returnType) {
        result += "\n" + indentStr(indent + 1) + "(Returns " + returnType->toString() + ")";
    }
    result += ")";
    return result;
}

std::string ImportDecl::toString(int indent) const {
    return indentStr(indent) + "(ImportDecl " + path + ")";
}

std::string ClassDecl::toString(int indent) const {
    std::string result = indentStr(indent) + "(ClassDecl " + (isPublic ? "public " : "") + (isShared ? "shared " : "") + name;
    if (!typeParams.empty()) {
        result += "<";
        for (size_t i = 0; i < typeParams.size(); i++) {
            if (i > 0) result += ", ";
            result += typeParams[i];
        }
        result += ">";
    }
    if (!baseClass.empty()) {
        result += " : " + baseClass;
    }
    for (const auto& iface : interfaces) {
        result += " : " + iface;
    }
    for (const auto& f : fields) {
        result += "\n" + f->toString(indent + 1);
    }
    for (const auto& m : methods) {
        result += "\n" + m->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string InterfaceDecl::toString(int indent) const {
    std::string result = indentStr(indent) + "(InterfaceDecl " + name;
    for (const auto& m : methods) {
        result += "\n" + m->toString(indent + 1);
    }
    result += ")";
    return result;
}

std::string EnumDecl::toString(int indent) const {
    std::string result = indentStr(indent) + "(EnumDecl " + name;
    for (const auto& c : cases) {
        result += "\n" + indentStr(indent + 1) + "(Case " + c + ")";
    }
    result += ")";
    return result;
}

std::string MatchExpr::toString(int indent) const {
    std::string result = indentStr(indent) + "(MatchExpr\n";
    result += subject->toString(indent + 1);
    for (const auto& arm : arms) {
        result += "\n" + indentStr(indent + 1) + "(Arm ";
        if (!arm.enumName.empty()) result += arm.enumName + ".";
        result += arm.caseName;
        if (arm.body) result += "\n" + arm.body->toString(indent + 2);
        result += ")";
    }
    result += ")";
    return result;
}

std::string Program::toString() const {
    std::string result = "(Program";
    for (const auto& decl : declarations) {
        result += "\n" + decl->toString(1);
    }
    result += ")";
    return result;
}

} // namespace chris
