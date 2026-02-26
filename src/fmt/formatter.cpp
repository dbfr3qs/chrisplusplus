#include "fmt/formatter.h"
#include <sstream>

namespace chris {

Formatter::Formatter(int indentWidth) : indentWidth_(indentWidth) {}

std::string Formatter::ind(int level) const {
    return std::string(level * indentWidth_, ' ');
}

std::string Formatter::formatTypeExpr(const TypeExpr& type) {
    return type.toString();
}

std::string Formatter::formatAnnotations(const std::vector<Annotation>& annotations, int indent) {
    std::string result;
    for (auto& ann : annotations) {
        result += ind(indent) + "@" + ann.name;
        if (!ann.arguments.empty()) {
            result += "(";
            for (size_t i = 0; i < ann.arguments.size(); i++) {
                if (i > 0) result += ", ";
                result += "\"" + ann.arguments[i] + "\"";
            }
            result += ")";
        }
        result += "\n";
    }
    return result;
}

std::string Formatter::formatParams(const std::vector<Parameter>& params) {
    std::string result;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) result += ", ";
        result += params[i].name;
        if (params[i].type) {
            result += ": " + formatTypeExpr(*params[i].type);
        }
    }
    return result;
}

// --- Program ---

std::string Formatter::format(const Program& program) {
    std::string result;
    bool lastWasImport = false;
    bool firstDecl = true;

    for (auto& decl : program.declarations) {
        bool isImport = dynamic_cast<ImportDecl*>(decl.get()) != nullptr;
        bool isFunc = dynamic_cast<FuncDecl*>(decl.get()) != nullptr;
        bool isClass = dynamic_cast<ClassDecl*>(decl.get()) != nullptr;
        bool isIface = dynamic_cast<InterfaceDecl*>(decl.get()) != nullptr;
        bool isEnum = dynamic_cast<EnumDecl*>(decl.get()) != nullptr;
        bool isExtern = dynamic_cast<ExternFuncDecl*>(decl.get()) != nullptr;

        // Add blank line between different declaration groups
        if (!firstDecl) {
            if (lastWasImport && !isImport) {
                result += "\n";
            } else if (isFunc || isClass || isIface || isEnum) {
                result += "\n";
            }
        }

        result += formatStmt(*decl, 0);
        firstDecl = false;
        lastWasImport = isImport;
    }

    // Ensure trailing newline
    if (!result.empty() && result.back() != '\n') {
        result += "\n";
    }

    return result;
}

// --- Statements ---

std::string Formatter::formatStmt(const Stmt& stmt, int indent) {
    if (auto* func = dynamic_cast<const FuncDecl*>(&stmt)) {
        return formatFuncDecl(*func, indent);
    } else if (auto* ext = dynamic_cast<const ExternFuncDecl*>(&stmt)) {
        return formatExternFuncDecl(*ext, indent);
    } else if (auto* var = dynamic_cast<const VarDecl*>(&stmt)) {
        return formatVarDecl(*var, indent);
    } else if (auto* cls = dynamic_cast<const ClassDecl*>(&stmt)) {
        return formatClassDecl(*cls, indent);
    } else if (auto* iface = dynamic_cast<const InterfaceDecl*>(&stmt)) {
        return formatInterfaceDecl(*iface, indent);
    } else if (auto* enm = dynamic_cast<const EnumDecl*>(&stmt)) {
        return formatEnumDecl(*enm, indent);
    } else if (auto* imp = dynamic_cast<const ImportDecl*>(&stmt)) {
        return formatImportDecl(*imp, indent);
    } else if (auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        return formatIfStmt(*ifStmt, indent);
    } else if (auto* whileStmt = dynamic_cast<const WhileStmt*>(&stmt)) {
        return formatWhileStmt(*whileStmt, indent);
    } else if (auto* forStmt = dynamic_cast<const ForStmt*>(&stmt)) {
        return formatForStmt(*forStmt, indent);
    } else if (auto* retStmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
        return formatReturnStmt(*retStmt, indent);
    } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(&stmt)) {
        return formatExprStmt(*exprStmt, indent);
    } else if (auto* throwStmt = dynamic_cast<const ThrowStmt*>(&stmt)) {
        return formatThrowStmt(*throwStmt, indent);
    } else if (auto* tryCatch = dynamic_cast<const TryCatchStmt*>(&stmt)) {
        return formatTryCatchStmt(*tryCatch, indent);
    } else if (auto* unsafeBlock = dynamic_cast<const UnsafeBlock*>(&stmt)) {
        return formatUnsafeBlock(*unsafeBlock, indent);
    } else if (dynamic_cast<const BreakStmt*>(&stmt)) {
        return ind(indent) + "break;\n";
    } else if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        return ind(indent) + "continue;\n";
    } else if (auto* block = dynamic_cast<const Block*>(&stmt)) {
        return formatBlock(*block, indent);
    }
    return "";
}

std::string Formatter::formatBlock(const Block& block, int indent) {
    std::string result;
    for (auto& stmt : block.statements) {
        result += formatStmt(*stmt, indent);
    }
    return result;
}

// --- Declarations ---

std::string Formatter::formatFuncDecl(const FuncDecl& func, int indent) {
    std::string result = formatAnnotations(func.annotations, indent);
    result += ind(indent);

    // Access modifier (only emit for class methods at indent > 0)
    if (indent > 0) {
        switch (func.access) {
            case AccessModifier::Public: result += "public "; break;
            case AccessModifier::Protected: result += "protected "; break;
            case AccessModifier::Private: break; // default, don't emit
        }
    }

    if (func.isAsync) result += "async ";
    if (func.isOperator) {
        result += "func operator" + func.name + "(";
    } else {
        result += "func " + func.name + "(";
    }

    result += formatParams(func.parameters) + ")";

    if (func.returnType) {
        result += " -> ";
        if (func.asyncKind == AsyncKind::Io) result += "io ";
        else if (func.asyncKind == AsyncKind::Compute) result += "compute ";
        result += formatTypeExpr(*func.returnType);
    }

    result += " {\n";
    if (func.body) {
        result += formatBlock(*func.body, indent + 1);
    }
    result += ind(indent) + "}\n";

    return result;
}

std::string Formatter::formatExternFuncDecl(const ExternFuncDecl& func, int indent) {
    std::string result = ind(indent) + "extern func " + func.name + "(";
    result += formatParams(func.parameters);
    if (func.isVariadic) {
        if (!func.parameters.empty()) result += ", ";
        result += "...";
    }
    result += ")";
    if (func.returnType) {
        result += " -> " + formatTypeExpr(*func.returnType);
    }
    result += ";\n";
    return result;
}

std::string Formatter::formatVarDecl(const VarDecl& decl, int indent) {
    std::string result = ind(indent);

    // Access modifier for class fields
    if (indent > 0 && decl.access != AccessModifier::Private) {
        switch (decl.access) {
            case AccessModifier::Public: result += "public "; break;
            case AccessModifier::Protected: result += "protected "; break;
            default: break;
        }
    }

    result += decl.isMutable ? "var " : "let ";
    result += decl.name;

    if (decl.typeAnnotation) {
        result += ": " + formatTypeExpr(*decl.typeAnnotation);
    }
    if (decl.initializer) {
        result += " = " + formatExpr(*decl.initializer);
    }
    result += ";\n";
    return result;
}

std::string Formatter::formatClassDecl(const ClassDecl& decl, int indent) {
    std::string result = formatAnnotations(decl.annotations, indent);
    result += ind(indent);
    if (decl.isPublic) result += "public ";
    if (decl.isShared) result += "shared ";
    result += "class " + decl.name;

    if (!decl.typeParams.empty()) {
        result += "<";
        for (size_t i = 0; i < decl.typeParams.size(); i++) {
            if (i > 0) result += ", ";
            result += decl.typeParams[i];
        }
        result += ">";
    }

    if (!decl.baseClass.empty()) {
        result += " : " + decl.baseClass;
    }

    if (!decl.interfaces.empty()) {
        result += decl.baseClass.empty() ? " : " : ", ";
        for (size_t i = 0; i < decl.interfaces.size(); i++) {
            if (i > 0) result += ", ";
            result += decl.interfaces[i];
        }
    }

    result += " {\n";

    for (auto& field : decl.fields) {
        result += formatVarDecl(*field, indent + 1);
    }

    if (!decl.fields.empty() && !decl.methods.empty()) {
        result += "\n";
    }

    for (size_t i = 0; i < decl.methods.size(); i++) {
        if (i > 0) result += "\n";
        result += formatFuncDecl(*decl.methods[i], indent + 1);
    }

    result += ind(indent) + "}\n";
    return result;
}

std::string Formatter::formatInterfaceDecl(const InterfaceDecl& decl, int indent) {
    std::string result = formatAnnotations(decl.annotations, indent);
    result += ind(indent) + "interface " + decl.name + " {\n";
    for (auto& method : decl.methods) {
        result += ind(indent + 1) + "func " + method->name + "(";
        result += formatParams(method->parameters) + ")";
        if (method->returnType) {
            result += " -> " + formatTypeExpr(*method->returnType);
        }
        result += ";\n";
    }
    result += ind(indent) + "}\n";
    return result;
}

std::string Formatter::formatEnumDecl(const EnumDecl& decl, int indent) {
    std::string result = formatAnnotations(decl.annotations, indent);
    result += ind(indent) + "enum " + decl.name + " {\n";
    // Prefer variants if available
    if (!decl.variants.empty()) {
        for (auto& variant : decl.variants) {
            result += ind(indent + 1) + "case " + variant.name;
            if (variant.associatedType) {
                result += "(" + formatTypeExpr(*variant.associatedType) + ")";
            }
            result += ";\n";
        }
    } else {
        for (auto& c : decl.cases) {
            result += ind(indent + 1) + "case " + c + ";\n";
        }
    }
    result += ind(indent) + "}\n";
    return result;
}

std::string Formatter::formatImportDecl(const ImportDecl& decl, int indent) {
    return ind(indent) + "import " + decl.path + ";\n";
}

// --- Statements ---

std::string Formatter::formatIfStmt(const IfStmt& stmt, int indent) {
    std::string result = ind(indent) + "if " + formatExpr(*stmt.condition) + " {\n";
    if (stmt.thenBlock) {
        result += formatBlock(*stmt.thenBlock, indent + 1);
    }
    if (stmt.elseBlock) {
        if (auto* elseIf = dynamic_cast<const IfStmt*>(stmt.elseBlock.get())) {
            result += ind(indent) + "} else ";
            // Format the else-if without leading indent (it's on same line as } else)
            std::string elseIfStr = formatIfStmt(*elseIf, indent);
            // Strip the leading indent from the else-if
            size_t start = elseIfStr.find_first_not_of(' ');
            if (start != std::string::npos) {
                result += elseIfStr.substr(start);
            }
        } else if (auto* elseBlock = dynamic_cast<const Block*>(stmt.elseBlock.get())) {
            result += ind(indent) + "} else {\n";
            result += formatBlock(*elseBlock, indent + 1);
            result += ind(indent) + "}\n";
        }
    } else {
        result += ind(indent) + "}\n";
    }
    return result;
}

std::string Formatter::formatWhileStmt(const WhileStmt& stmt, int indent) {
    std::string result = ind(indent) + "while " + formatExpr(*stmt.condition) + " {\n";
    if (stmt.body) {
        result += formatBlock(*stmt.body, indent + 1);
    }
    result += ind(indent) + "}\n";
    return result;
}

std::string Formatter::formatForStmt(const ForStmt& stmt, int indent) {
    std::string result = ind(indent) + "for " + stmt.variable + " in " + formatExpr(*stmt.iterable) + " {\n";
    if (stmt.body) {
        result += formatBlock(*stmt.body, indent + 1);
    }
    result += ind(indent) + "}\n";
    return result;
}

std::string Formatter::formatReturnStmt(const ReturnStmt& stmt, int indent) {
    if (stmt.value) {
        return ind(indent) + "return " + formatExpr(*stmt.value) + ";\n";
    }
    return ind(indent) + "return;\n";
}

std::string Formatter::formatExprStmt(const ExprStmt& stmt, int indent) {
    if (stmt.expression) {
        std::string expr = formatExpr(*stmt.expression);
        // Match expressions don't need trailing semicolons
        if (dynamic_cast<const MatchExpr*>(stmt.expression.get())) {
            return ind(indent) + expr + "\n";
        }
        return ind(indent) + expr + ";\n";
    }
    return "";
}

std::string Formatter::formatThrowStmt(const ThrowStmt& stmt, int indent) {
    return ind(indent) + "throw " + formatExpr(*stmt.expression) + ";\n";
}

std::string Formatter::formatTryCatchStmt(const TryCatchStmt& stmt, int indent) {
    std::string result = ind(indent) + "try {\n";
    result += formatBlock(*stmt.tryBlock, indent + 1);

    for (auto& clause : stmt.catchClauses) {
        result += ind(indent) + "} catch (" + clause.varName + ": " + clause.typeName + ") {\n";
        result += formatBlock(*clause.body, indent + 1);
    }

    if (stmt.finallyBlock) {
        result += ind(indent) + "} finally {\n";
        result += formatBlock(*stmt.finallyBlock, indent + 1);
    }

    result += ind(indent) + "}\n";
    return result;
}

std::string Formatter::formatUnsafeBlock(const UnsafeBlock& stmt, int indent) {
    std::string result = ind(indent) + "unsafe {\n";
    if (stmt.body) {
        result += formatBlock(*stmt.body, indent + 1);
    }
    result += ind(indent) + "}\n";
    return result;
}

// --- Expressions ---

std::string Formatter::formatExpr(const Expr& expr) {
    if (auto* intLit = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        return std::to_string(intLit->value);
    } else if (auto* floatLit = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
        std::ostringstream oss;
        oss << floatLit->value;
        std::string s = oss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
            s += ".0";
        }
        return s;
    } else if (auto* strLit = dynamic_cast<const StringLiteralExpr*>(&expr)) {
        return "\"" + strLit->value + "\"";
    } else if (auto* charLit = dynamic_cast<const CharLiteralExpr*>(&expr)) {
        return std::string("'") + charLit->value + "'";
    } else if (auto* boolLit = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        return boolLit->value ? "true" : "false";
    } else if (dynamic_cast<const NilLiteralExpr*>(&expr)) {
        return "nil";
    } else if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        return id->name;
    } else if (dynamic_cast<const ThisExpr*>(&expr)) {
        return "this";
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        return formatBinaryExpr(*bin);
    } else if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        return formatUnaryExpr(*un);
    } else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        return formatCallExpr(*call);
    } else if (auto* member = dynamic_cast<const MemberExpr*>(&expr)) {
        return formatMemberExpr(*member);
    } else if (auto* construct = dynamic_cast<const ConstructExpr*>(&expr)) {
        return formatConstructExpr(*construct);
    } else if (auto* assign = dynamic_cast<const AssignExpr*>(&expr)) {
        return formatAssignExpr(*assign);
    } else if (auto* range = dynamic_cast<const RangeExpr*>(&expr)) {
        return formatExpr(*range->start) + ".." + formatExpr(*range->end);
    } else if (auto* nilCoalesce = dynamic_cast<const NilCoalesceExpr*>(&expr)) {
        return formatExpr(*nilCoalesce->value) + " ?? " + formatExpr(*nilCoalesce->defaultValue);
    } else if (auto* forceUnwrap = dynamic_cast<const ForceUnwrapExpr*>(&expr)) {
        return formatExpr(*forceUnwrap->operand) + "!";
    } else if (auto* optChain = dynamic_cast<const OptionalChainExpr*>(&expr)) {
        return formatExpr(*optChain->object) + "?." + optChain->member;
    } else if (auto* interp = dynamic_cast<const StringInterpolationExpr*>(&expr)) {
        return formatStringInterpolation(*interp);
    } else if (auto* arr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        return formatArrayLiteral(*arr);
    } else if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        return formatIndexExpr(*idx);
    } else if (auto* lambda = dynamic_cast<const LambdaExpr*>(&expr)) {
        return formatLambdaExpr(*lambda);
    } else if (auto* await = dynamic_cast<const AwaitExpr*>(&expr)) {
        return "await " + formatExpr(*await->operand);
    } else if (auto* ifExpr = dynamic_cast<const IfExpr*>(&expr)) {
        return "if " + formatExpr(*ifExpr->condition) + " then " +
               formatExpr(*ifExpr->thenExpr) + " else " + formatExpr(*ifExpr->elseExpr);
    } else if (auto* match = dynamic_cast<const MatchExpr*>(&expr)) {
        return formatMatchExpr(*match);
    }
    return "/* unknown expr */";
}

std::string Formatter::formatBinaryExpr(const BinaryExpr& expr) {
    return formatExpr(*expr.left) + " " + expr.op + " " + formatExpr(*expr.right);
}

std::string Formatter::formatUnaryExpr(const UnaryExpr& expr) {
    return expr.op + formatExpr(*expr.operand);
}

std::string Formatter::formatCallExpr(const CallExpr& expr) {
    std::string result = formatExpr(*expr.callee) + "(";
    for (size_t i = 0; i < expr.arguments.size(); i++) {
        if (i > 0) result += ", ";
        result += formatExpr(*expr.arguments[i]);
    }
    result += ")";
    return result;
}

std::string Formatter::formatMemberExpr(const MemberExpr& expr) {
    return formatExpr(*expr.object) + "." + expr.member;
}

std::string Formatter::formatConstructExpr(const ConstructExpr& expr) {
    std::string result = expr.className + " { ";
    for (size_t i = 0; i < expr.fieldInits.size(); i++) {
        if (i > 0) result += ", ";
        result += expr.fieldInits[i].first + ": " + formatExpr(*expr.fieldInits[i].second);
    }
    result += " }";
    return result;
}

std::string Formatter::formatAssignExpr(const AssignExpr& expr) {
    return formatExpr(*expr.target) + " = " + formatExpr(*expr.value);
}

std::string Formatter::formatLambdaExpr(const LambdaExpr& expr) {
    std::string result = "(";
    for (size_t i = 0; i < expr.params.size(); i++) {
        if (i > 0) result += ", ";
        result += expr.params[i].name;
        if (expr.params[i].type) {
            result += ": " + formatTypeExpr(*expr.params[i].type);
        }
    }
    result += ") => ";

    if (expr.bodyExpr) {
        result += formatExpr(*expr.bodyExpr);
    } else if (expr.bodyBlock) {
        result += "{\n";
        result += formatBlock(*expr.bodyBlock, 1);
        result += "}";
    }
    return result;
}

std::string Formatter::formatMatchExpr(const MatchExpr& expr) {
    std::string result = "match " + formatExpr(*expr.subject) + " {\n";
    for (auto& arm : expr.arms) {
        result += "    ";
        if (!arm.enumName.empty()) {
            result += arm.enumName + ".";
        }
        result += arm.caseName;
        if (!arm.bindingName.empty()) {
            result += "(" + arm.bindingName + ")";
        }
        result += " => ";
        if (auto* block = dynamic_cast<const Block*>(arm.body.get())) {
            result += "{\n";
            result += formatBlock(*block, 2);
            result += "    }\n";
        } else if (auto* exprStmt = dynamic_cast<const ExprStmt*>(arm.body.get())) {
            result += formatExpr(*exprStmt->expression) + ",\n";
        } else {
            result += formatStmt(*arm.body, 0);
        }
    }
    result += "}";
    return result;
}

std::string Formatter::formatStringInterpolation(const StringInterpolationExpr& expr) {
    std::string result = "\"";
    for (size_t i = 0; i < expr.parts.size(); i++) {
        result += expr.parts[i];
        if (i < expr.expressions.size()) {
            result += "${" + formatExpr(*expr.expressions[i]) + "}";
        }
    }
    result += "\"";
    return result;
}

std::string Formatter::formatArrayLiteral(const ArrayLiteralExpr& expr) {
    std::string result = "[";
    for (size_t i = 0; i < expr.elements.size(); i++) {
        if (i > 0) result += ", ";
        result += formatExpr(*expr.elements[i]);
    }
    result += "]";
    return result;
}

std::string Formatter::formatIndexExpr(const IndexExpr& expr) {
    return formatExpr(*expr.object) + "[" + formatExpr(*expr.index) + "]";
}

} // namespace chris
