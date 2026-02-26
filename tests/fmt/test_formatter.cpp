#include <gtest/gtest.h>
#include "fmt/formatter.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "common/diagnostic.h"

using namespace chris;

class FormatterTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    std::string formatSource(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        EXPECT_FALSE(diag.hasErrors());
        Formatter formatter;
        return formatter.format(program);
    }
};

// --- Variable declarations ---

TEST_F(FormatterTest, VarDecl) {
    auto result = formatSource("var x: Int = 42;");
    EXPECT_EQ(result, "var x: Int = 42;\n");
}

TEST_F(FormatterTest, LetDecl) {
    auto result = formatSource("let name: String = \"hello\";");
    EXPECT_EQ(result, "let name: String = \"hello\";\n");
}

// --- Functions ---

TEST_F(FormatterTest, SimpleFunction) {
    auto result = formatSource(
        "func add(a: Int, b: Int) -> Int {\n"
        "return a + b;\n"
        "}\n"
    );
    EXPECT_NE(result.find("func add(a: Int, b: Int) -> Int {"), std::string::npos);
    EXPECT_NE(result.find("    return a + b;"), std::string::npos);
    EXPECT_NE(result.find("}"), std::string::npos);
}

TEST_F(FormatterTest, FunctionNoReturn) {
    auto result = formatSource(
        "func greet(name: String) {\n"
        "print(name);\n"
        "}\n"
    );
    EXPECT_NE(result.find("func greet(name: String) {"), std::string::npos);
    EXPECT_NE(result.find("    print(name);"), std::string::npos);
}

TEST_F(FormatterTest, ExternFunction) {
    auto result = formatSource("extern func puts(s: String) -> Int;");
    EXPECT_EQ(result, "extern func puts(s: String) -> Int;\n");
}

TEST_F(FormatterTest, ExternFunctionNoReturn) {
    auto result = formatSource("extern func exit(code: Int);");
    EXPECT_EQ(result, "extern func exit(code: Int);\n");
}

// --- If/else ---

TEST_F(FormatterTest, IfStatement) {
    auto result = formatSource(
        "func f() {\n"
        "if x > 0 { return x; }\n"
        "}\n"
    );
    EXPECT_NE(result.find("    if x > 0 {"), std::string::npos);
    EXPECT_NE(result.find("        return x;"), std::string::npos);
}

TEST_F(FormatterTest, IfElseStatement) {
    auto result = formatSource(
        "func f() {\n"
        "if x > 0 { return x; } else { return 0; }\n"
        "}\n"
    );
    EXPECT_NE(result.find("} else {"), std::string::npos);
}

// --- Loops ---

TEST_F(FormatterTest, WhileLoop) {
    auto result = formatSource(
        "func f() {\n"
        "while x > 0 { x = x - 1; }\n"
        "}\n"
    );
    EXPECT_NE(result.find("    while x > 0 {"), std::string::npos);
}

TEST_F(FormatterTest, ForLoop) {
    auto result = formatSource(
        "func f() {\n"
        "for i in 0..10 { print(i); }\n"
        "}\n"
    );
    EXPECT_NE(result.find("    for i in 0..10 {"), std::string::npos);
}

// --- Classes ---

TEST_F(FormatterTest, ClassDecl) {
    auto result = formatSource(
        "class Point {\n"
        "public var x: Int;\n"
        "public var y: Int;\n"
        "}\n"
    );
    EXPECT_NE(result.find("class Point {"), std::string::npos);
    EXPECT_NE(result.find("    public var x: Int;"), std::string::npos);
}

TEST_F(FormatterTest, SharedClass) {
    auto result = formatSource(
        "shared class Counter {\n"
        "public var count: Int;\n"
        "}\n"
    );
    EXPECT_NE(result.find("shared class Counter {"), std::string::npos);
}

// --- Enum ---

TEST_F(FormatterTest, EnumDecl) {
    auto result = formatSource(
        "enum Color {\n"
        "case Red,\n"
        "case Green,\n"
        "case Blue\n"
        "}\n"
    );
    EXPECT_NE(result.find("enum Color {"), std::string::npos);
    EXPECT_NE(result.find("    case Red;"), std::string::npos);
}

// --- Try/catch ---

TEST_F(FormatterTest, TryCatch) {
    auto result = formatSource(
        "func f() {\n"
        "try { throw \"error\"; } catch (e: Error) { print(e); }\n"
        "}\n"
    );
    EXPECT_NE(result.find("    try {"), std::string::npos);
    EXPECT_NE(result.find("    } catch (e: Error) {"), std::string::npos);
}

// --- Unsafe block ---

TEST_F(FormatterTest, UnsafeBlock) {
    auto result = formatSource(
        "func f() {\n"
        "unsafe { var x = 42; }\n"
        "}\n"
    );
    EXPECT_NE(result.find("    unsafe {"), std::string::npos);
    EXPECT_NE(result.find("var x"), std::string::npos);
}

// --- Import ---

TEST_F(FormatterTest, ImportDecl) {
    auto result = formatSource("import std.io;");
    EXPECT_EQ(result, "import std.io;\n");
}

// --- Expressions ---

TEST_F(FormatterTest, StringInterpolation) {
    auto result = formatSource(
        "func f() {\n"
        "let msg = \"hello ${name} world\";\n"
        "}\n"
    );
    EXPECT_NE(result.find("${name}"), std::string::npos);
}

TEST_F(FormatterTest, ArrayLiteral) {
    auto result = formatSource(
        "func f() {\n"
        "let arr = [1, 2, 3];\n"
        "}\n"
    );
    EXPECT_NE(result.find("[1, 2, 3]"), std::string::npos);
}

TEST_F(FormatterTest, ConstructExpr) {
    auto result = formatSource(
        "class Point { public var x: Int; public var y: Int; }\n"
        "func f() {\n"
        "let p = Point { x: 1, y: 2 };\n"
        "}\n"
    );
    EXPECT_NE(result.find("Point { x: 1, y: 2 }"), std::string::npos);
}

// --- Idempotency ---

TEST_F(FormatterTest, IdempotentFormatting) {
    std::string source =
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    if x > 0 {\n"
        "        print(x);\n"
        "    }\n"
        "}\n";
    auto first = formatSource(source);
    // Format the formatted output again
    DiagnosticEngine diag2;
    Lexer lexer2(first, "test.chr", diag2);
    auto tokens2 = lexer2.tokenize();
    Parser parser2(tokens2, diag2);
    auto program2 = parser2.parse();
    EXPECT_FALSE(diag2.hasErrors());
    Formatter formatter2;
    auto second = formatter2.format(program2);
    EXPECT_EQ(first, second);
}
