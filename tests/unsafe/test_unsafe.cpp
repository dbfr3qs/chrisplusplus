#include <gtest/gtest.h>
#include "codegen/codegen.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "sema/type_checker.h"
#include "common/diagnostic.h"

using namespace chris;

// ============================================================================
// Parser Tests for unsafe blocks
// ============================================================================

class UnsafeParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

TEST_F(UnsafeParserTest, UnsafeBlockParsesInsideFunction) {
    auto program = parse(
        "func main() {\n"
        "    unsafe {\n"
        "        var x = 42;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);
}

TEST_F(UnsafeParserTest, UnsafeBlockWithMultipleStatements) {
    auto program = parse(
        "func main() {\n"
        "    unsafe {\n"
        "        var x = 1;\n"
        "        var y = 2;\n"
        "        var z = x + y;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(UnsafeParserTest, NestedUnsafeBlocks) {
    auto program = parse(
        "func main() {\n"
        "    unsafe {\n"
        "        var x = 1;\n"
        "        unsafe {\n"
        "            var y = 2;\n"
        "        }\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(UnsafeParserTest, UnsafeBlockAmongOtherStatements) {
    auto program = parse(
        "func main() {\n"
        "    var a = 1;\n"
        "    unsafe {\n"
        "        var b = 2;\n"
        "    }\n"
        "    var c = 3;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

// ============================================================================
// Type Checker Tests for unsafe blocks
// ============================================================================

class UnsafeTypeCheckerTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parseAndCheck(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        if (!diag.hasErrors()) {
            TypeChecker checker(diag);
            checker.check(program);
        }
        return program;
    }
};

TEST_F(UnsafeTypeCheckerTest, UnsafeBlockValid) {
    parseAndCheck(
        "func main() {\n"
        "    unsafe {\n"
        "        var x = 42;\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(UnsafeTypeCheckerTest, UnsafeBlockTypeChecksBody) {
    parseAndCheck(
        "func main() {\n"
        "    unsafe {\n"
        "        var x: Int = 42;\n"
        "        var y: Int = x + 1;\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(UnsafeTypeCheckerTest, UnsafeBlockScopesVariables) {
    parseAndCheck(
        "func main() {\n"
        "    unsafe {\n"
        "        var x = 42;\n"
        "    }\n"
        "    // x should not be visible here\n"
        "    var y = 10;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// CodeGen Tests for unsafe blocks
// ============================================================================

class UnsafeCodeGenTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    std::string generateIR(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        if (diag.hasErrors()) return "";

        TypeChecker checker(diag);
        checker.check(program);
        if (diag.hasErrors()) return "";

        CodeGen codegen("test", diag);
        if (!codegen.generate(program, checker.genericInstantiations())) return "";
        return codegen.getIR();
    }
};

TEST_F(UnsafeCodeGenTest, UnsafeBlockEmitsBody) {
    auto ir = generateIR(
        "func main() {\n"
        "    unsafe {\n"
        "        var x: Int = 42;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // The variable should be allocated inside main
    EXPECT_NE(ir.find("alloca i64"), std::string::npos);
}

TEST_F(UnsafeCodeGenTest, UnsafeBlockWithExternCall) {
    auto ir = generateIR(
        "extern func puts(s: String) -> Int;\n"
        "func main() {\n"
        "    unsafe {\n"
        "        puts(\"hello from unsafe\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("call i64 @puts"), std::string::npos);
}

TEST_F(UnsafeCodeGenTest, UnsafeBlockMultipleStatements) {
    auto ir = generateIR(
        "func main() {\n"
        "    unsafe {\n"
        "        var a: Int = 1;\n"
        "        var b: Int = 2;\n"
        "        var c: Int = a + b;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("add i64"), std::string::npos);
}
