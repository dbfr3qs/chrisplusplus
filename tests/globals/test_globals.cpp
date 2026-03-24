#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Parser Tests ====================

class GlobalsParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

TEST_F(GlobalsParserTest, TopLevelVarDecl) {
    auto program = parse("var x = 42; func main() {}");
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 2u);
    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(varDecl, nullptr);
    EXPECT_EQ(varDecl->name, "x");
    EXPECT_TRUE(varDecl->isMutable);
}

TEST_F(GlobalsParserTest, TopLevelLetDecl) {
    auto program = parse("let MAX = 100; func main() {}");
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 2u);
    auto* varDecl = dynamic_cast<VarDecl*>(program.declarations[0].get());
    ASSERT_NE(varDecl, nullptr);
    EXPECT_EQ(varDecl->name, "MAX");
    EXPECT_FALSE(varDecl->isMutable);
}

TEST_F(GlobalsParserTest, MultipleGlobals) {
    auto program = parse("var a = 1; var b = 2; let c = 3; func main() {}");
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 4u);
}

// ==================== Type Checker Tests ====================

class GlobalsTypeCheckerTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    void check(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        TypeChecker checker(diag);
        checker.check(program);
    }
};

TEST_F(GlobalsTypeCheckerTest, GlobalVarAccessInFunction) {
    check("var x = 42; func main() { print(x); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(GlobalsTypeCheckerTest, GlobalLetAccessInFunction) {
    check("let MAX = 100; func main() { print(MAX); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(GlobalsTypeCheckerTest, GlobalVarAssignInFunction) {
    check("var x = 0; func main() { x = 42; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(GlobalsTypeCheckerTest, GlobalStringVar) {
    check("var name = \"world\"; func main() { print(name); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(GlobalsTypeCheckerTest, GlobalFloatVar) {
    check("var pi = 3.14; func main() { print(pi); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(GlobalsTypeCheckerTest, GlobalBoolVar) {
    check("var flag = true; func main() { print(flag); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(GlobalsTypeCheckerTest, GlobalAccessFromMultipleFunctions) {
    check("var counter = 0; func inc() { counter = counter + 1; } func main() { inc(); print(counter); }");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class GlobalsCodegenTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    std::string getIR(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        TypeChecker checker(diag);
        checker.check(program);
        CodeGen codegen("test_module", diag);
        codegen.generate(program);
        return codegen.getIR();
    }
};

TEST_F(GlobalsCodegenTest, GlobalVarEmitsGlobalVariable) {
    auto ir = getIR("var counter = 0; func main() { print(counter); }");
    EXPECT_NE(ir.find("@global.counter"), std::string::npos);
}

TEST_F(GlobalsCodegenTest, GlobalStringEmitsGlobalVariable) {
    auto ir = getIR("var name = \"hello\"; func main() { print(name); }");
    EXPECT_NE(ir.find("@global.name"), std::string::npos);
}

TEST_F(GlobalsCodegenTest, GlobalInitFunctionEmitted) {
    auto ir = getIR("var x = 42; func main() { print(x); }");
    EXPECT_NE(ir.find("__chris_init_globals"), std::string::npos);
}

TEST_F(GlobalsCodegenTest, GlobalInitCalledInMain) {
    auto ir = getIR("var x = 42; func main() { print(x); }");
    // __chris_init_globals should be called in main
    EXPECT_NE(ir.find("call void @__chris_init_globals()"), std::string::npos);
}

TEST_F(GlobalsCodegenTest, NoInitFunctionWithoutGlobals) {
    auto ir = getIR("func main() { print(42); }");
    EXPECT_EQ(ir.find("__chris_init_globals"), std::string::npos);
}

TEST_F(GlobalsCodegenTest, GlobalVarStoreInFunction) {
    auto ir = getIR("var x = 0; func set() { x = 42; } func main() { set(); }");
    // Should have a store to @global.x
    EXPECT_NE(ir.find("@global.x"), std::string::npos);
}
