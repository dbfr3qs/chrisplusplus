#include <gtest/gtest.h>
#include "codegen/codegen.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "sema/type_checker.h"
#include "fmt/formatter.h"
#include "common/diagnostic.h"

using namespace chris;

// ============================================================================
// Parser Tests for Annotations
// ============================================================================

class AnnotationParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

TEST_F(AnnotationParserTest, SimpleAnnotationOnFunc) {
    auto program = parse(
        "@Test\n"
        "func myTest() -> Int {\n"
        "    return 1;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "myTest");
    ASSERT_EQ(func->annotations.size(), 1u);
    EXPECT_EQ(func->annotations[0].name, "Test");
    EXPECT_TRUE(func->annotations[0].arguments.empty());
}

TEST_F(AnnotationParserTest, AnnotationWithStringArg) {
    auto program = parse(
        "@Deprecated(\"Use newMethod instead\")\n"
        "func oldMethod() {\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->annotations.size(), 1u);
    EXPECT_EQ(func->annotations[0].name, "Deprecated");
    ASSERT_EQ(func->annotations[0].arguments.size(), 1u);
    EXPECT_EQ(func->annotations[0].arguments[0], "Use newMethod instead");
}

TEST_F(AnnotationParserTest, MultipleAnnotationsOnFunc) {
    auto program = parse(
        "@Deprecated(\"old\")\n"
        "@Inline\n"
        "func doThing() {\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->annotations.size(), 2u);
    EXPECT_EQ(func->annotations[0].name, "Deprecated");
    EXPECT_EQ(func->annotations[1].name, "Inline");
}

TEST_F(AnnotationParserTest, AnnotationOnClass) {
    auto program = parse(
        "@Serializable\n"
        "class Config {\n"
        "    public var host: String;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->name, "Config");
    ASSERT_EQ(cls->annotations.size(), 1u);
    EXPECT_EQ(cls->annotations[0].name, "Serializable");
}

TEST_F(AnnotationParserTest, AnnotationOnPublicClass) {
    auto program = parse(
        "@CLayout\n"
        "public class CStruct {\n"
        "    public var x: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->name, "CStruct");
    EXPECT_TRUE(cls->isPublic);
    ASSERT_EQ(cls->annotations.size(), 1u);
    EXPECT_EQ(cls->annotations[0].name, "CLayout");
}

TEST_F(AnnotationParserTest, AnnotationOnSharedClass) {
    auto program = parse(
        "@Serializable\n"
        "shared class SafeConfig {\n"
        "    public var port: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_TRUE(cls->isShared);
    ASSERT_EQ(cls->annotations.size(), 1u);
    EXPECT_EQ(cls->annotations[0].name, "Serializable");
}

TEST_F(AnnotationParserTest, AnnotationOnPublicSharedClass) {
    auto program = parse(
        "@Serializable\n"
        "public shared class SafeConfig {\n"
        "    public var port: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_TRUE(cls->isPublic);
    EXPECT_TRUE(cls->isShared);
    ASSERT_EQ(cls->annotations.size(), 1u);
    EXPECT_EQ(cls->annotations[0].name, "Serializable");
}

TEST_F(AnnotationParserTest, AnnotationOnInterface) {
    auto program = parse(
        "@Deprecated(\"use NewInterface\")\n"
        "interface OldInterface {\n"
        "    func doSomething();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* iface = dynamic_cast<InterfaceDecl*>(program.declarations[0].get());
    ASSERT_NE(iface, nullptr);
    ASSERT_EQ(iface->annotations.size(), 1u);
    EXPECT_EQ(iface->annotations[0].name, "Deprecated");
}

TEST_F(AnnotationParserTest, AnnotationOnEnum) {
    auto program = parse(
        "@Deprecated(\"use NewStatus\")\n"
        "enum Status {\n"
        "    case Active\n"
        "    case Inactive\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* en = dynamic_cast<EnumDecl*>(program.declarations[0].get());
    ASSERT_NE(en, nullptr);
    ASSERT_EQ(en->annotations.size(), 1u);
    EXPECT_EQ(en->annotations[0].name, "Deprecated");
}

TEST_F(AnnotationParserTest, AnnotationOnAsyncFunc) {
    auto program = parse(
        "@Test\n"
        "async func fetchData() -> io Int {\n"
        "    return 42;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_TRUE(func->isAsync);
    ASSERT_EQ(func->annotations.size(), 1u);
    EXPECT_EQ(func->annotations[0].name, "Test");
}

TEST_F(AnnotationParserTest, AnnotationOnPublicFunc) {
    auto program = parse(
        "@Deprecated(\"use newFunc\")\n"
        "public func oldFunc() {\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->annotations.size(), 1u);
    EXPECT_EQ(func->annotations[0].name, "Deprecated");
}

TEST_F(AnnotationParserTest, AnnotationWithMultipleArgs) {
    auto program = parse(
        "@Deprecated(\"old\", \"use newFunc\")\n"
        "func old() {\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    ASSERT_EQ(func->annotations[0].arguments.size(), 2u);
    EXPECT_EQ(func->annotations[0].arguments[0], "old");
    EXPECT_EQ(func->annotations[0].arguments[1], "use newFunc");
}

TEST_F(AnnotationParserTest, NoAnnotation) {
    auto program = parse(
        "func plain() {\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_TRUE(func->annotations.empty());
}

// ============================================================================
// Type Checker Tests for Annotations
// ============================================================================

class AnnotationTypeCheckerTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parseAndCheck(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        TypeChecker checker(diag);
        checker.check(program);
        return program;
    }
};

TEST_F(AnnotationTypeCheckerTest, KnownAnnotationNoError) {
    parseAndCheck(
        "@Deprecated(\"use new\")\n"
        "func oldFunc() -> Int {\n"
        "    return 1;\n"
        "}\n"
        "func main() -> Int {\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(AnnotationTypeCheckerTest, UnknownAnnotationWarning) {
    parseAndCheck(
        "@FooBar\n"
        "func test() {\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_GT(diag.warningCount(), 0u);
}

TEST_F(AnnotationTypeCheckerTest, InvalidAnnotationTarget) {
    parseAndCheck(
        "@CLayout\n"
        "func test() {\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(AnnotationTypeCheckerTest, DeprecatedCallSiteWarning) {
    parseAndCheck(
        "@Deprecated(\"Use newMethod instead\")\n"
        "func oldMethod() -> Int {\n"
        "    return 1;\n"
        "}\n"
        "func main() -> Int {\n"
        "    return oldMethod();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_GT(diag.warningCount(), 0u);
}

TEST_F(AnnotationTypeCheckerTest, DeprecatedNoCallNoWarning) {
    parseAndCheck(
        "@Deprecated(\"old\")\n"
        "func oldMethod() -> Int {\n"
        "    return 1;\n"
        "}\n"
        "func main() -> Int {\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
    EXPECT_EQ(diag.warningCount(), 0u);
}

TEST_F(AnnotationTypeCheckerTest, SerializableOnClass) {
    parseAndCheck(
        "@Serializable\n"
        "class Config {\n"
        "    public var host: String;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(AnnotationTypeCheckerTest, SerializableOnFuncIsError) {
    parseAndCheck(
        "@Serializable\n"
        "func test() {\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(AnnotationTypeCheckerTest, TestOnFunc) {
    parseAndCheck(
        "@Test\n"
        "func testSomething() {\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(AnnotationTypeCheckerTest, TestOnClassIsError) {
    parseAndCheck(
        "@Test\n"
        "class Foo {\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

// ============================================================================
// Formatter Tests for Annotations
// ============================================================================

class AnnotationFormatterTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;
    Formatter formatter;

    std::string formatSource(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        return formatter.format(program);
    }
};

TEST_F(AnnotationFormatterTest, SimpleAnnotation) {
    auto result = formatSource(
        "@Test\n"
        "func myTest() {\n"
        "}\n"
    );
    EXPECT_NE(result.find("@Test"), std::string::npos);
    EXPECT_NE(result.find("func myTest()"), std::string::npos);
    // Annotation should be on a separate line before func
    auto atPos = result.find("@Test");
    auto funcPos = result.find("func myTest");
    EXPECT_LT(atPos, funcPos);
}

TEST_F(AnnotationFormatterTest, AnnotationWithArgs) {
    auto result = formatSource(
        "@Deprecated(\"Use newMethod\")\n"
        "func old() {\n"
        "}\n"
    );
    EXPECT_NE(result.find("@Deprecated(\"Use newMethod\")"), std::string::npos);
}

TEST_F(AnnotationFormatterTest, MultipleAnnotations) {
    auto result = formatSource(
        "@Deprecated(\"old\")\n"
        "@Inline\n"
        "func doThing() {\n"
        "}\n"
    );
    EXPECT_NE(result.find("@Deprecated(\"old\")"), std::string::npos);
    EXPECT_NE(result.find("@Inline"), std::string::npos);
}

TEST_F(AnnotationFormatterTest, AnnotationOnClass) {
    auto result = formatSource(
        "@Serializable\n"
        "class Config {\n"
        "    public var host: String;\n"
        "}\n"
    );
    EXPECT_NE(result.find("@Serializable"), std::string::npos);
    auto atPos = result.find("@Serializable");
    auto classPos = result.find("class Config");
    EXPECT_LT(atPos, classPos);
}

TEST_F(AnnotationFormatterTest, AnnotationOnEnum) {
    auto result = formatSource(
        "@Deprecated(\"use NewStatus\")\n"
        "enum Status {\n"
        "    case Active\n"
        "}\n"
    );
    EXPECT_NE(result.find("@Deprecated(\"use NewStatus\")"), std::string::npos);
    auto atPos = result.find("@Deprecated");
    auto enumPos = result.find("enum Status");
    EXPECT_LT(atPos, enumPos);
}

TEST_F(AnnotationFormatterTest, Idempotent) {
    std::string source =
        "@Deprecated(\"old\")\n"
        "func oldFunc() {\n"
        "}\n";
    auto first = formatSource(source);
    // Format again
    DiagnosticEngine diag2;
    Lexer lexer2(first, "test.chr", diag2);
    auto tokens2 = lexer2.tokenize();
    Parser parser2(tokens2, diag2);
    auto program2 = parser2.parse();
    auto second = formatter.format(program2);
    EXPECT_EQ(first, second);
}
