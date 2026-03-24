#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Type Checker Tests ====================

class PtrTypeCheckerTest : public ::testing::Test {
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

TEST_F(PtrTypeCheckerTest, PtrTypeAnnotation) {
    check("func test(p: Ptr) {}");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(PtrTypeCheckerTest, PtrGenericTypeAnnotation) {
    check("func test(p: Ptr<Int>) {}");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(PtrTypeCheckerTest, PtrArithmeticInUnsafe) {
    check("func main() { var p: Ptr = nil; unsafe { var q = p + 4; } }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(PtrTypeCheckerTest, PtrArithmeticOutsideUnsafeErrors) {
    check("func main() { var p: Ptr = nil; var q = p + 4; }");
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(PtrTypeCheckerTest, PtrSubtractionInUnsafe) {
    check("func main() { var p: Ptr = nil; unsafe { var q = p - 8; } }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(PtrTypeCheckerTest, PtrAsParameter) {
    check("func process(data: Ptr<Int>, size: Int) {} func main() {}");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(PtrTypeCheckerTest, PtrAsReturnType) {
    check("func getData() -> Ptr { return nil; } func main() {}");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class PtrCodegenTest : public ::testing::Test {
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

TEST_F(PtrCodegenTest, PtrParameterIsPointerType) {
    auto ir = getIR("func test(p: Ptr) -> Ptr { return p; }");
    // The parameter and return should be ptr type
    EXPECT_NE(ir.find("ptr"), std::string::npos);
}

TEST_F(PtrCodegenTest, PtrArithmeticEmitsGEP) {
    auto ir = getIR("func test(p: Ptr, n: Int) -> Ptr { unsafe { return p + n; } }");
    EXPECT_NE(ir.find("getelementptr"), std::string::npos);
}

TEST_F(PtrCodegenTest, PtrSubtractionEmitsGEP) {
    auto ir = getIR("func test(p: Ptr, n: Int) -> Ptr { unsafe { return p - n; } }");
    EXPECT_NE(ir.find("getelementptr"), std::string::npos);
}
