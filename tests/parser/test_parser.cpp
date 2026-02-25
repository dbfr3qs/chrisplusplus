#include <gtest/gtest.h>
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "common/diagnostic.h"

using namespace chris;

class ParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

// --- Variable Declarations ---

TEST_F(ParserTest, VarDeclWithInference) {
    auto program = parse("var x = 42;");
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(varDecl, nullptr);
    EXPECT_EQ(varDecl->name, "x");
    EXPECT_TRUE(varDecl->isMutable);
    EXPECT_EQ(varDecl->typeAnnotation, nullptr);
    ASSERT_NE(varDecl->initializer, nullptr);

    auto* intLit = dynamic_cast<IntLiteralExpr*>(varDecl->initializer.get());
    ASSERT_NE(intLit, nullptr);
    EXPECT_EQ(intLit->value, 42);
}

TEST_F(ParserTest, VarDeclWithType) {
    auto program = parse("var x: Int = 42;");
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(varDecl, nullptr);
    EXPECT_EQ(varDecl->name, "x");
    ASSERT_NE(varDecl->typeAnnotation, nullptr);

    auto* namedType = dynamic_cast<NamedType*>(varDecl->typeAnnotation.get());
    ASSERT_NE(namedType, nullptr);
    EXPECT_EQ(namedType->name, "Int");
    EXPECT_FALSE(namedType->nullable);
}

TEST_F(ParserTest, LetDecl) {
    auto program = parse("let name = \"Chris\";");
    ASSERT_FALSE(diag.hasErrors());

    auto* letDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(letDecl, nullptr);
    EXPECT_EQ(letDecl->name, "name");
    EXPECT_FALSE(letDecl->isMutable);

    auto* strLit = dynamic_cast<StringLiteralExpr*>(letDecl->initializer.get());
    ASSERT_NE(strLit, nullptr);
    EXPECT_EQ(strLit->value, "Chris");
}

TEST_F(ParserTest, NullableType) {
    auto program = parse("var x: Int? = nil;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(varDecl, nullptr);

    auto* namedType = dynamic_cast<NamedType*>(varDecl->typeAnnotation.get());
    ASSERT_NE(namedType, nullptr);
    EXPECT_EQ(namedType->name, "Int");
    EXPECT_TRUE(namedType->nullable);

    auto* nilLit = dynamic_cast<NilLiteralExpr*>(varDecl->initializer.get());
    ASSERT_NE(nilLit, nullptr);
}

// --- Function Declarations ---

TEST_F(ParserTest, EmptyMainFunction) {
    auto program = parse("func main() { }");
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "main");
    EXPECT_TRUE(func->parameters.empty());
    EXPECT_EQ(func->returnType, nullptr);
    EXPECT_TRUE(func->body->statements.empty());
}

TEST_F(ParserTest, FunctionWithParams) {
    auto program = parse("func add(a: Int, b: Int) -> Int { return a + b; }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "add");
    ASSERT_EQ(func->parameters.size(), 2u);
    EXPECT_EQ(func->parameters[0].name, "a");
    EXPECT_EQ(func->parameters[1].name, "b");

    auto* retType = dynamic_cast<NamedType*>(func->returnType.get());
    ASSERT_NE(retType, nullptr);
    EXPECT_EQ(retType->name, "Int");

    ASSERT_EQ(func->body->statements.size(), 1u);
    auto* retStmt = dynamic_cast<ReturnStmt*>(func->body->statements[0].get());
    ASSERT_NE(retStmt, nullptr);

    auto* binExpr = dynamic_cast<BinaryExpr*>(retStmt->value.get());
    ASSERT_NE(binExpr, nullptr);
    EXPECT_EQ(binExpr->op, "+");
}

// --- Expressions ---

TEST_F(ParserTest, BinaryArithmetic) {
    auto program = parse("var x = 1 + 2 * 3;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(varDecl, nullptr);

    // Should be (1 + (2 * 3)) due to precedence
    auto* add = dynamic_cast<BinaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->op, "+");

    auto* left = dynamic_cast<IntLiteralExpr*>(add->left.get());
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->value, 1);

    auto* mul = dynamic_cast<BinaryExpr*>(add->right.get());
    ASSERT_NE(mul, nullptr);
    EXPECT_EQ(mul->op, "*");
}

TEST_F(ParserTest, UnaryNegation) {
    auto program = parse("var x = -42;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* unary = dynamic_cast<UnaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op, "-");

    auto* lit = dynamic_cast<IntLiteralExpr*>(unary->operand.get());
    ASSERT_NE(lit, nullptr);
    EXPECT_EQ(lit->value, 42);
}

TEST_F(ParserTest, UnaryNot) {
    auto program = parse("var x = !true;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* unary = dynamic_cast<UnaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(unary, nullptr);
    EXPECT_EQ(unary->op, "!");
}

TEST_F(ParserTest, Comparison) {
    auto program = parse("var x = a > 10;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, ">");
}

TEST_F(ParserTest, LogicalAnd) {
    auto program = parse("var x = a && b;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, "&&");
}

TEST_F(ParserTest, LogicalOr) {
    auto program = parse("var x = a || b;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* bin = dynamic_cast<BinaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(bin, nullptr);
    EXPECT_EQ(bin->op, "||");
}

TEST_F(ParserTest, ParenthesizedExpression) {
    auto program = parse("var x = (1 + 2) * 3;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* mul = dynamic_cast<BinaryExpr*>(varDecl->initializer.get());
    ASSERT_NE(mul, nullptr);
    EXPECT_EQ(mul->op, "*");

    auto* add = dynamic_cast<BinaryExpr*>(mul->left.get());
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add->op, "+");
}

TEST_F(ParserTest, FunctionCall) {
    auto program = parse("print(\"hello\");");
    ASSERT_FALSE(diag.hasErrors());

    auto* exprStmt = dynamic_cast<ExprStmt*>(program.declarations[0].get());
    ASSERT_NE(exprStmt, nullptr);

    auto* call = dynamic_cast<CallExpr*>(exprStmt->expression.get());
    ASSERT_NE(call, nullptr);

    auto* callee = dynamic_cast<IdentifierExpr*>(call->callee.get());
    ASSERT_NE(callee, nullptr);
    EXPECT_EQ(callee->name, "print");

    ASSERT_EQ(call->arguments.size(), 1u);
    auto* arg = dynamic_cast<StringLiteralExpr*>(call->arguments[0].get());
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->value, "hello");
}

TEST_F(ParserTest, FunctionCallMultipleArgs) {
    auto program = parse("add(1, 2);");
    ASSERT_FALSE(diag.hasErrors());

    auto* exprStmt = dynamic_cast<ExprStmt*>(program.declarations[0].get());
    auto* call = dynamic_cast<CallExpr*>(exprStmt->expression.get());
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->arguments.size(), 2u);
}

TEST_F(ParserTest, MemberAccess) {
    auto program = parse("var x = obj.field;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* member = dynamic_cast<MemberExpr*>(varDecl->initializer.get());
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->member, "field");

    auto* obj = dynamic_cast<IdentifierExpr*>(member->object.get());
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->name, "obj");
}

TEST_F(ParserTest, MethodCall) {
    auto program = parse("obj.method(42);");
    ASSERT_FALSE(diag.hasErrors());

    auto* exprStmt = dynamic_cast<ExprStmt*>(program.declarations[0].get());
    auto* call = dynamic_cast<CallExpr*>(exprStmt->expression.get());
    ASSERT_NE(call, nullptr);

    auto* member = dynamic_cast<MemberExpr*>(call->callee.get());
    ASSERT_NE(member, nullptr);
    EXPECT_EQ(member->member, "method");
}

TEST_F(ParserTest, Assignment) {
    auto program = parse("x = 42;");
    ASSERT_FALSE(diag.hasErrors());

    auto* exprStmt = dynamic_cast<ExprStmt*>(program.declarations[0].get());
    auto* assign = dynamic_cast<AssignExpr*>(exprStmt->expression.get());
    ASSERT_NE(assign, nullptr);

    auto* target = dynamic_cast<IdentifierExpr*>(assign->target.get());
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->name, "x");
}

// --- String Interpolation ---

TEST_F(ParserTest, StringInterpolation) {
    auto program = parse("var x = \"hello ${name}!\";");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* interp = dynamic_cast<StringInterpolationExpr*>(varDecl->initializer.get());
    ASSERT_NE(interp, nullptr);
    ASSERT_EQ(interp->parts.size(), 2u);
    EXPECT_EQ(interp->parts[0], "hello ");
    EXPECT_EQ(interp->parts[1], "!");
    ASSERT_EQ(interp->expressions.size(), 1u);

    auto* ident = dynamic_cast<IdentifierExpr*>(interp->expressions[0].get());
    ASSERT_NE(ident, nullptr);
    EXPECT_EQ(ident->name, "name");
}

TEST_F(ParserTest, StringInterpolationMultiple) {
    auto program = parse("var x = \"${a} and ${b}\";");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* interp = dynamic_cast<StringInterpolationExpr*>(varDecl->initializer.get());
    ASSERT_NE(interp, nullptr);
    ASSERT_EQ(interp->parts.size(), 3u);
    ASSERT_EQ(interp->expressions.size(), 2u);
    EXPECT_EQ(interp->parts[0], "");
    EXPECT_EQ(interp->parts[1], " and ");
    EXPECT_EQ(interp->parts[2], "");
}

// --- Control Flow ---

TEST_F(ParserTest, IfStatement) {
    auto program = parse("func main() { if x > 10 { print(\"big\"); } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->body->statements.size(), 1u);

    auto* ifStmt = dynamic_cast<IfStmt*>(func->body->statements[0].get());
    ASSERT_NE(ifStmt, nullptr);
    EXPECT_NE(ifStmt->condition, nullptr);
    EXPECT_NE(ifStmt->thenBlock, nullptr);
    EXPECT_EQ(ifStmt->elseBlock, nullptr);
}

TEST_F(ParserTest, IfElseStatement) {
    auto program = parse("func main() { if x > 10 { } else { } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* ifStmt = dynamic_cast<IfStmt*>(func->body->statements[0].get());
    ASSERT_NE(ifStmt, nullptr);
    EXPECT_NE(ifStmt->elseBlock, nullptr);
}

TEST_F(ParserTest, IfElseIfStatement) {
    auto program = parse("func main() { if x > 10 { } else if x > 5 { } else { } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* ifStmt = dynamic_cast<IfStmt*>(func->body->statements[0].get());
    ASSERT_NE(ifStmt, nullptr);

    auto* elseIf = dynamic_cast<IfStmt*>(ifStmt->elseBlock.get());
    ASSERT_NE(elseIf, nullptr);
    EXPECT_NE(elseIf->elseBlock, nullptr);
}

TEST_F(ParserTest, WhileLoop) {
    auto program = parse("func main() { while x > 0 { x = x - 1; } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* whileStmt = dynamic_cast<WhileStmt*>(func->body->statements[0].get());
    ASSERT_NE(whileStmt, nullptr);
    EXPECT_NE(whileStmt->condition, nullptr);
    EXPECT_NE(whileStmt->body, nullptr);
}

TEST_F(ParserTest, ForInLoop) {
    auto program = parse("func main() { for i in items { print(i); } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* forStmt = dynamic_cast<ForStmt*>(func->body->statements[0].get());
    ASSERT_NE(forStmt, nullptr);
    EXPECT_EQ(forStmt->variable, "i");
    EXPECT_NE(forStmt->iterable, nullptr);
    EXPECT_NE(forStmt->body, nullptr);
}

TEST_F(ParserTest, BreakAndContinue) {
    auto program = parse("func main() { while true { break; continue; } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* whileStmt = dynamic_cast<WhileStmt*>(func->body->statements[0].get());
    ASSERT_EQ(whileStmt->body->statements.size(), 2u);

    EXPECT_NE(dynamic_cast<BreakStmt*>(whileStmt->body->statements[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<ContinueStmt*>(whileStmt->body->statements[1].get()), nullptr);
}

TEST_F(ParserTest, ReturnStatement) {
    auto program = parse("func foo() -> Int { return 42; }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* retStmt = dynamic_cast<ReturnStmt*>(func->body->statements[0].get());
    ASSERT_NE(retStmt, nullptr);

    auto* intLit = dynamic_cast<IntLiteralExpr*>(retStmt->value.get());
    ASSERT_NE(intLit, nullptr);
    EXPECT_EQ(intLit->value, 42);
}

TEST_F(ParserTest, ReturnVoid) {
    auto program = parse("func foo() { return; }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* retStmt = dynamic_cast<ReturnStmt*>(func->body->statements[0].get());
    ASSERT_NE(retStmt, nullptr);
    EXPECT_EQ(retStmt->value, nullptr);
}

// --- Import ---

TEST_F(ParserTest, ImportStatement) {
    auto program = parse("import std.io;");
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* importDecl = dynamic_cast<ImportDecl*>(program.declarations[0].get());
    ASSERT_NE(importDecl, nullptr);
    EXPECT_EQ(importDecl->path, "std.io");
}

// --- Comments ---

TEST_F(ParserTest, CommentsAreSkipped) {
    auto program = parse(
        "// This is a comment\n"
        "func main() {\n"
        "    /* block comment */\n"
        "    var x = 42;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->body->statements.size(), 1u);
}

// --- Pretty Printer ---

TEST_F(ParserTest, PrettyPrintSimple) {
    auto program = parse("var x = 42;");
    ASSERT_FALSE(diag.hasErrors());
    std::string output = program.toString();
    EXPECT_NE(output.find("VarDecl x"), std::string::npos);
    EXPECT_NE(output.find("IntLiteral 42"), std::string::npos);
}

TEST_F(ParserTest, PrettyPrintFunction) {
    auto program = parse("func main() { }");
    ASSERT_FALSE(diag.hasErrors());
    std::string output = program.toString();
    EXPECT_NE(output.find("FuncDecl main"), std::string::npos);
    EXPECT_NE(output.find("Params"), std::string::npos);
    EXPECT_NE(output.find("Block"), std::string::npos);
}

// --- Done Criterion: Full Program ---

TEST_F(ParserTest, DoneCriterionProgram) {
    auto program = parse(
        "func main() {\n"
        "    var name = \"Chris\";\n"
        "    var x: Int = 42;\n"
        "    if x > 10 {\n"
        "        print(\"Hello, ${name}! x is ${x}\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Parser produced errors for the done criterion program";
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "main");
    ASSERT_EQ(func->body->statements.size(), 3u);

    // var name = "Chris";
    auto* nameDecl = dynamic_cast<VarDecl*>(func->body->statements[0].get());
    ASSERT_NE(nameDecl, nullptr);
    EXPECT_EQ(nameDecl->name, "name");

    // var x: Int = 42;
    auto* xDecl = dynamic_cast<VarDecl*>(func->body->statements[1].get());
    ASSERT_NE(xDecl, nullptr);
    EXPECT_EQ(xDecl->name, "x");
    auto* xType = dynamic_cast<NamedType*>(xDecl->typeAnnotation.get());
    ASSERT_NE(xType, nullptr);
    EXPECT_EQ(xType->name, "Int");

    // if x > 10 { print("Hello, ${name}! x is ${x}"); }
    auto* ifStmt = dynamic_cast<IfStmt*>(func->body->statements[2].get());
    ASSERT_NE(ifStmt, nullptr);

    auto* cond = dynamic_cast<BinaryExpr*>(ifStmt->condition.get());
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->op, ">");

    ASSERT_EQ(ifStmt->thenBlock->statements.size(), 1u);
    auto* printStmt = dynamic_cast<ExprStmt*>(ifStmt->thenBlock->statements[0].get());
    ASSERT_NE(printStmt, nullptr);

    auto* printCall = dynamic_cast<CallExpr*>(printStmt->expression.get());
    ASSERT_NE(printCall, nullptr);
    ASSERT_EQ(printCall->arguments.size(), 1u);

    auto* interp = dynamic_cast<StringInterpolationExpr*>(printCall->arguments[0].get());
    ASSERT_NE(interp, nullptr);
    ASSERT_EQ(interp->parts.size(), 3u);
    EXPECT_EQ(interp->parts[0], "Hello, ");
    EXPECT_EQ(interp->parts[1], "! x is ");
    EXPECT_EQ(interp->parts[2], "");
    ASSERT_EQ(interp->expressions.size(), 2u);

    // Pretty-print should work
    std::string output = program.toString();
    EXPECT_NE(output.find("FuncDecl main"), std::string::npos);
    EXPECT_NE(output.find("StringInterpolation"), std::string::npos);
}

// --- Bool Literals ---

TEST_F(ParserTest, BoolLiterals) {
    auto program = parse("var x = true; var y = false;");
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 2u);

    auto* xDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* boolTrue = dynamic_cast<BoolLiteralExpr*>(xDecl->initializer.get());
    ASSERT_NE(boolTrue, nullptr);
    EXPECT_TRUE(boolTrue->value);

    auto* yDecl = dynamic_cast<VarDecl*>(program.declarations[1].get());
    auto* boolFalse = dynamic_cast<BoolLiteralExpr*>(yDecl->initializer.get());
    ASSERT_NE(boolFalse, nullptr);
    EXPECT_FALSE(boolFalse->value);
}

// --- Float Literals ---

TEST_F(ParserTest, FloatLiteral) {
    auto program = parse("var pi = 3.14;");
    ASSERT_FALSE(diag.hasErrors());

    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    auto* floatLit = dynamic_cast<FloatLiteralExpr*>(varDecl->initializer.get());
    ASSERT_NE(floatLit, nullptr);
    EXPECT_DOUBLE_EQ(floatLit->value, 3.14);
}

// --- Multiple Declarations ---

TEST_F(ParserTest, MultipleTopLevelDecls) {
    auto program = parse(
        "import std.io;\n"
        "func greet(name: String) { print(name); }\n"
        "func main() { greet(\"Chris\"); }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 3u);

    EXPECT_NE(dynamic_cast<ImportDecl*>(program.declarations[0].get()), nullptr);
    EXPECT_NE(dynamic_cast<FuncDecl*>(program.declarations[1].get()), nullptr);
    EXPECT_NE(dynamic_cast<FuncDecl*>(program.declarations[2].get()), nullptr);
}

// --- Phase 5: Range Expressions ---

TEST_F(ParserTest, RangeExpression) {
    auto program = parse("func main() { for i in 0..10 { } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* forStmt = dynamic_cast<ForStmt*>(func->body->statements[0].get());
    ASSERT_NE(forStmt, nullptr);
    EXPECT_EQ(forStmt->variable, "i");

    auto* range = dynamic_cast<RangeExpr*>(forStmt->iterable.get());
    ASSERT_NE(range, nullptr);

    auto* start = dynamic_cast<IntLiteralExpr*>(range->start.get());
    ASSERT_NE(start, nullptr);
    EXPECT_EQ(start->value, 0);

    auto* end = dynamic_cast<IntLiteralExpr*>(range->end.get());
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end->value, 10);
}

TEST_F(ParserTest, RangeWithExpressions) {
    auto program = parse("func main() { var n = 10; for i in 0..n { } }");
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* forStmt = dynamic_cast<ForStmt*>(func->body->statements[1].get());
    ASSERT_NE(forStmt, nullptr);

    auto* range = dynamic_cast<RangeExpr*>(forStmt->iterable.get());
    ASSERT_NE(range, nullptr);

    auto* end = dynamic_cast<IdentifierExpr*>(range->end.get());
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end->name, "n");
}

TEST_F(ParserTest, RecursiveFunction) {
    auto program = parse(
        "func fib(n: Int) -> Int {\n"
        "    if n <= 1 { return n; }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "fib");
    ASSERT_EQ(func->parameters.size(), 1u);
    ASSERT_EQ(func->body->statements.size(), 2u);
}

TEST_F(ParserTest, FibonacciDoneCriterion) {
    auto program = parse(
        "func fibonacci(n: Int) -> Int {\n"
        "    if n <= 1 { return n; }\n"
        "    return fibonacci(n - 1) + fibonacci(n - 2);\n"
        "}\n"
        "func main() {\n"
        "    for i in 0..10 {\n"
        "        print(\"${fibonacci(i)}\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Parser failed on Phase 5 done criterion";
    ASSERT_EQ(program.declarations.size(), 2u);
}

// --- Phase 6: Classes ---

TEST_F(ParserTest, ClassDeclaration) {
    auto program = parse(
        "class Point {\n"
        "    var x: Int;\n"
        "    var y: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->name, "Point");
    ASSERT_EQ(cls->fields.size(), 2u);
    EXPECT_EQ(cls->fields[0]->name, "x");
    EXPECT_EQ(cls->fields[1]->name, "y");
}

TEST_F(ParserTest, ClassWithMethods) {
    auto program = parse(
        "class Point {\n"
        "    var x: Int;\n"
        "    func getX() -> Int { return this.x; }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->fields.size(), 1u);
    ASSERT_EQ(cls->methods.size(), 1u);
    EXPECT_EQ(cls->methods[0]->name, "getX");
}

TEST_F(ParserTest, ClassConstructor) {
    auto program = parse(
        "class Point {\n"
        "    var x: Int;\n"
        "    func new(x: Int) -> Point {\n"
        "        return Point { x: x };\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->methods.size(), 1u);
    EXPECT_EQ(cls->methods[0]->name, "new");
}

TEST_F(ParserTest, PublicClass) {
    auto program = parse(
        "public class Widget {\n"
        "    var id: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_TRUE(cls->isPublic);
    EXPECT_EQ(cls->name, "Widget");
}

TEST_F(ParserTest, ConstructExpression) {
    auto program = parse(
        "class Point { var x: Int; var y: Int; }\n"
        "func main() { var p = Point { x: 1, y: 2 }; }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

// --- Phase 7: Inheritance & Interfaces ---

TEST_F(ParserTest, InterfaceDeclaration) {
    auto program = parse(
        "interface Printable {\n"
        "    func toString() -> String;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* iface = dynamic_cast<InterfaceDecl*>(program.declarations[0].get());
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->name, "Printable");
    ASSERT_EQ(iface->methods.size(), 1u);
    EXPECT_EQ(iface->methods[0]->name, "toString");
    EXPECT_EQ(iface->methods[0]->body, nullptr);
}

TEST_F(ParserTest, ClassInheritance) {
    auto program = parse(
        "class Animal {\n"
        "    var name: String;\n"
        "}\n"
        "class Dog : Animal {\n"
        "    var breed: String;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* dog = dynamic_cast<ClassDecl*>(program.declarations[1].get());
    ASSERT_NE(dog, nullptr);
    EXPECT_EQ(dog->name, "Dog");
    EXPECT_EQ(dog->baseClass, "Animal");
}

TEST_F(ParserTest, ClassImplementsInterface) {
    auto program = parse(
        "interface Printable {\n"
        "    func toString() -> String;\n"
        "}\n"
        "class Animal : Printable {\n"
        "    var name: String;\n"
        "    func toString() -> String { return this.name; }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* animal = dynamic_cast<ClassDecl*>(program.declarations[1].get());
    ASSERT_NE(animal, nullptr);
    EXPECT_EQ(animal->baseClass, "Printable");
}

TEST_F(ParserTest, ClassInheritanceWithInterface) {
    auto program = parse(
        "interface Printable {\n"
        "    func toString() -> String;\n"
        "}\n"
        "class Animal {\n"
        "    var name: String;\n"
        "}\n"
        "class Dog : Animal, Printable {\n"
        "    func toString() -> String { return this.name; }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* dog = dynamic_cast<ClassDecl*>(program.declarations[2].get());
    ASSERT_NE(dog, nullptr);
    EXPECT_EQ(dog->baseClass, "Animal");
    ASSERT_EQ(dog->interfaces.size(), 1u);
    EXPECT_EQ(dog->interfaces[0], "Printable");
}

// --- Phase 8: Null Safety ---

TEST_F(ParserTest, NilCoalesceOperator) {
    auto program = parse("func main() { var x: Int? = nil; var y = x ?? 0; }");
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, ForceUnwrapOperator) {
    auto program = parse("func main() { var x: Int? = 42; var y = x!; }");
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, OptionalChainOperator) {
    auto program = parse(
        "class Point { var x: Int; var y: Int; }\n"
        "func main() { var p: Point? = nil; var x = p?.x; }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, NilCoalesceAST) {
    auto program = parse("func main() { var x: Int? = nil; var y = x ?? 5; }");
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    // Second statement is var y = x ?? 5
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[1].get());
    ASSERT_NE(varDecl, nullptr);
    auto* coalesce = dynamic_cast<NilCoalesceExpr*>(varDecl->initializer.get());
    ASSERT_NE(coalesce, nullptr);
}

TEST_F(ParserTest, ForceUnwrapAST) {
    auto program = parse("func main() { var x: Int? = 42; var y = x!; }");
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[1].get());
    ASSERT_NE(varDecl, nullptr);
    auto* unwrap = dynamic_cast<ForceUnwrapExpr*>(varDecl->initializer.get());
    ASSERT_NE(unwrap, nullptr);
}

TEST_F(ParserTest, OptionalChainAST) {
    auto program = parse(
        "class Point { var x: Int; var y: Int; }\n"
        "func main() { var p: Point? = nil; var x = p?.x; }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[1].get());
    ASSERT_NE(func, nullptr);
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[1].get());
    ASSERT_NE(varDecl, nullptr);
    auto* chain = dynamic_cast<OptionalChainExpr*>(varDecl->initializer.get());
    ASSERT_NE(chain, nullptr);
    EXPECT_EQ(chain->member, "x");
}

// --- Phase 9: Enums & Pattern Matching ---

TEST_F(ParserTest, EnumDeclaration) {
    auto program = parse("enum Color { Red, Green, Blue }");
    ASSERT_FALSE(diag.hasErrors());
    auto* enm = dynamic_cast<EnumDecl*>(program.declarations[0].get());
    ASSERT_NE(enm, nullptr);
    EXPECT_EQ(enm->name, "Color");
    ASSERT_EQ(enm->cases.size(), 3u);
    EXPECT_EQ(enm->cases[0], "Red");
    EXPECT_EQ(enm->cases[1], "Green");
    EXPECT_EQ(enm->cases[2], "Blue");
}

TEST_F(ParserTest, MatchExpression) {
    auto program = parse(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Red;\n"
        "    match c {\n"
        "        Color.Red => print(\"red\")\n"
        "        Color.Green => print(\"green\")\n"
        "        Color.Blue => print(\"blue\")\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, MatchExpressionAST) {
    auto program = parse(
        "enum Dir { Up, Down }\n"
        "func main() {\n"
        "    var d = Dir.Up;\n"
        "    match d {\n"
        "        Dir.Up => print(\"up\")\n"
        "        Dir.Down => print(\"down\")\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[1].get());
    ASSERT_NE(func, nullptr);
    // Third statement is the match
    auto* exprStmt = dynamic_cast<ExprStmt*>(func->body->statements[1].get());
    ASSERT_NE(exprStmt, nullptr);
    auto* matchExpr = dynamic_cast<MatchExpr*>(exprStmt->expression.get());
    ASSERT_NE(matchExpr, nullptr);
    ASSERT_EQ(matchExpr->arms.size(), 2u);
    EXPECT_EQ(matchExpr->arms[0].enumName, "Dir");
    EXPECT_EQ(matchExpr->arms[0].caseName, "Up");
    EXPECT_EQ(matchExpr->arms[1].caseName, "Down");
}

// --- Phase 10: Lambdas & Closures ---

TEST_F(ParserTest, LambdaExpression) {
    auto program = parse("func main() { var f = (x: Int) => x * 2; }");
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[0].get());
    ASSERT_NE(varDecl, nullptr);
    auto* lambda = dynamic_cast<LambdaExpr*>(varDecl->initializer.get());
    ASSERT_NE(lambda, nullptr);
    ASSERT_EQ(lambda->params.size(), 1u);
    EXPECT_EQ(lambda->params[0].name, "x");
    EXPECT_NE(lambda->bodyExpr, nullptr);
}

TEST_F(ParserTest, LambdaMultipleParams) {
    auto program = parse("func main() { var f = (a: Int, b: Int) => a + b; }");
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[0].get());
    auto* lambda = dynamic_cast<LambdaExpr*>(varDecl->initializer.get());
    ASSERT_NE(lambda, nullptr);
    ASSERT_EQ(lambda->params.size(), 2u);
    EXPECT_EQ(lambda->params[0].name, "a");
    EXPECT_EQ(lambda->params[1].name, "b");
}

TEST_F(ParserTest, LambdaZeroParams) {
    auto program = parse("func main() { var f = () => 42; }");
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[0].get());
    auto* lambda = dynamic_cast<LambdaExpr*>(varDecl->initializer.get());
    ASSERT_NE(lambda, nullptr);
    ASSERT_EQ(lambda->params.size(), 0u);
}

TEST_F(ParserTest, LambdaBlockBody) {
    auto program = parse(
        "func main() {\n"
        "    var f = (x: Int) => {\n"
        "        var y = x * 2;\n"
        "        return y + 1;\n"
        "    };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* varDecl = dynamic_cast<VarDecl*>(func->body->statements[0].get());
    auto* lambda = dynamic_cast<LambdaExpr*>(varDecl->initializer.get());
    ASSERT_NE(lambda, nullptr);
    EXPECT_NE(lambda->bodyBlock, nullptr);
    EXPECT_EQ(lambda->bodyExpr, nullptr);
}

// --- Phase 11: Match-as-expression & Error Handling ---

TEST_F(ParserTest, ThrowStatement) {
    auto program = parse("func main() { throw \"error\"; }");
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    auto* throwStmt = dynamic_cast<ThrowStmt*>(func->body->statements[0].get());
    ASSERT_NE(throwStmt, nullptr);
    EXPECT_NE(throwStmt->expression, nullptr);
}

TEST_F(ParserTest, TryCatchStatement) {
    auto program = parse(
        "func main() {\n"
        "    try {\n"
        "        throw \"oops\";\n"
        "    } catch (e: Error) {\n"
        "        print(e);\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* tryCatch = dynamic_cast<TryCatchStmt*>(func->body->statements[0].get());
    ASSERT_NE(tryCatch, nullptr);
    ASSERT_NE(tryCatch->tryBlock, nullptr);
    ASSERT_EQ(tryCatch->catchClauses.size(), 1u);
    EXPECT_EQ(tryCatch->catchClauses[0].varName, "e");
    EXPECT_EQ(tryCatch->catchClauses[0].typeName, "Error");
}

TEST_F(ParserTest, TryCatchFinally) {
    auto program = parse(
        "func main() {\n"
        "    try {\n"
        "        throw \"oops\";\n"
        "    } catch (e: Error) {\n"
        "        print(e);\n"
        "    } finally {\n"
        "        print(\"done\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    auto* tryCatch = dynamic_cast<TryCatchStmt*>(func->body->statements[0].get());
    ASSERT_NE(tryCatch, nullptr);
    ASSERT_NE(tryCatch->finallyBlock, nullptr);
}

// --- Phase 12: Access Modifiers ---

TEST_F(ParserTest, AccessModifierOnField) {
    auto program = parse(
        "class Foo {\n"
        "    public var x: Int;\n"
        "    private var y: Int;\n"
        "    protected var z: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->fields.size(), 3u);
    EXPECT_EQ(cls->fields[0]->access, AccessModifier::Public);
    EXPECT_EQ(cls->fields[1]->access, AccessModifier::Private);
    EXPECT_EQ(cls->fields[2]->access, AccessModifier::Protected);
}

TEST_F(ParserTest, AccessModifierOnMethod) {
    auto program = parse(
        "class Foo {\n"
        "    public func bar() { }\n"
        "    private func baz() { }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->methods.size(), 2u);
    EXPECT_EQ(cls->methods[0]->access, AccessModifier::Public);
    EXPECT_EQ(cls->methods[1]->access, AccessModifier::Private);
}

TEST_F(ParserTest, DefaultAccessIsPrivate) {
    auto program = parse(
        "class Foo {\n"
        "    var x: Int;\n"
        "    func bar() { }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->fields[0]->access, AccessModifier::Private);
    EXPECT_EQ(cls->methods[0]->access, AccessModifier::Private);
}

// --- Phase 13: Generics ---

TEST_F(ParserTest, GenericClassDeclaration) {
    auto program = parse(
        "class Box<T> {\n"
        "    public var value: T;\n"
        "    public func new(value: T) -> Box {\n"
        "        return Box { value: value };\n"
        "    }\n"
        "    public func get() -> T {\n"
        "        return this.value;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->name, "Box");
    ASSERT_EQ(cls->typeParams.size(), 1u);
    EXPECT_EQ(cls->typeParams[0], "T");
}

TEST_F(ParserTest, GenericClassMultipleParams) {
    auto program = parse(
        "class Pair<K, V> {\n"
        "    public var key: K;\n"
        "    public var val: V;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->typeParams.size(), 2u);
    EXPECT_EQ(cls->typeParams[0], "K");
    EXPECT_EQ(cls->typeParams[1], "V");
}

TEST_F(ParserTest, GenericTypeAnnotation) {
    auto program = parse(
        "class Box<T> { public var value: T; }\n"
        "func main() {\n"
        "    var b: Box<Int> = Box { value: 42 };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

// --- Phase 14: Operator Overloading ---

TEST_F(ParserTest, OperatorOverloadDeclaration) {
    auto program = parse(
        "class Vec {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Vec {\n"
        "        return Vec { x: x, y: y };\n"
        "    }\n"
        "    operator +(other: Vec) -> Vec {\n"
        "        return Vec.new(this.x + other.x, this.y + other.y);\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->methods.size(), 2u);
    EXPECT_EQ(cls->methods[1]->name, "operator+");
    EXPECT_TRUE(cls->methods[1]->isOperator);
}

TEST_F(ParserTest, MultipleOperatorOverloads) {
    auto program = parse(
        "class Vec {\n"
        "    public var x: Int;\n"
        "    operator +(other: Vec) -> Vec { return Vec { x: this.x + other.x }; }\n"
        "    operator ==(other: Vec) -> Bool { return this.x == other.x; }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    ASSERT_EQ(cls->methods.size(), 2u);
    EXPECT_EQ(cls->methods[0]->name, "operator+");
    EXPECT_EQ(cls->methods[1]->name, "operator==");
}

// --- Phase 15: Arrays ---

TEST_F(ParserTest, ArrayLiteral) {
    auto program = parse(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, ArrayIndexing) {
    auto program = parse(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    var x = arr[0];\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, ArrayLength) {
    auto program = parse(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    var len = arr.length;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, ForInArray) {
    auto program = parse(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    for x in arr {\n"
        "        print(x);\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(ParserTest, EmptyArrayLiteral) {
    auto program = parse(
        "func main() {\n"
        "    var arr = [];\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

// --- Phase 17: C FFI (extern functions) ---

TEST_F(ParserTest, ExternFuncDecl) {
    auto program = parse(
        "extern func puts(s: String) -> Int;\n"
        "func main() {\n"
        "    puts(\"hello from C\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 2u);
    auto* ext = dynamic_cast<ExternFuncDecl*>(program.declarations[0].get());
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->name, "puts");
    EXPECT_EQ(ext->parameters.size(), 1u);
    EXPECT_FALSE(ext->isVariadic);
}

TEST_F(ParserTest, ExternFuncNoReturn) {
    auto program = parse(
        "extern func abort();\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* ext = dynamic_cast<ExternFuncDecl*>(program.declarations[0].get());
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->name, "abort");
    EXPECT_EQ(ext->returnType, nullptr);
}

// --- Phase 18: Array Index Assignment ---

TEST_F(ParserTest, ArrayIndexAssignment) {
    auto program = parse(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    arr[0] = 99;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

// --- Phase 19: Multi-file Imports ---

TEST_F(ParserTest, ImportStringLiteral) {
    auto program = parse(
        "import \"mathlib.chr\";\n"
        "func main() {\n"
        "    print(1);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 2u);
    auto* imp = dynamic_cast<ImportDecl*>(program.declarations[0].get());
    ASSERT_NE(imp, nullptr);
    EXPECT_EQ(imp->path, "mathlib.chr");
}

TEST_F(ParserTest, ImportModulePath) {
    auto program = parse(
        "import std.io;\n"
        "func main() {}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    auto* imp = dynamic_cast<ImportDecl*>(program.declarations[0].get());
    ASSERT_NE(imp, nullptr);
    EXPECT_EQ(imp->path, "std.io");
}

TEST_F(ParserTest, MultipleImports) {
    auto program = parse(
        "import \"math.chr\";\n"
        "import \"utils.chr\";\n"
        "func main() {}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 3u);
}

// --- Phase 20: Type Conversion Methods ---

TEST_F(ParserTest, TypeConversionMethods) {
    auto program = parse(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var s = x.toString();\n"
        "    var f = x.toFloat();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}
