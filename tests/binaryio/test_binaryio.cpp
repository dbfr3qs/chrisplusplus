#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Type Checker Tests ====================

class BinaryIOTypeCheckerTest : public ::testing::Test {
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

TEST_F(BinaryIOTypeCheckerTest, Fopen) {
    check(R"(func main() { var f = fopen("test.bin", "rb"); })");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BinaryIOTypeCheckerTest, Fclose) {
    check(R"(func main() { var f = fopen("test.bin", "rb"); fclose(f); })");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BinaryIOTypeCheckerTest, FreadFwrite) {
    check(R"(
        func main() {
            var buf = alloc(1024);
            var f = fopen("test.bin", "rb");
            var n = fread(f, buf, 1024);
            fclose(f);
            dealloc(buf);
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BinaryIOTypeCheckerTest, FseekFtell) {
    check(R"(
        func main() {
            var f = fopen("test.bin", "rb");
            fseek(f, 0, 2);
            var size = ftell(f);
            fclose(f);
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BinaryIOTypeCheckerTest, Fsize) {
    check(R"(
        func main() {
            var f = fopen("test.bin", "rb");
            var size = fsize(f);
            fclose(f);
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BinaryIOTypeCheckerTest, AllocDealloc) {
    check(R"(func main() { var p = alloc(256); dealloc(p); })");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BinaryIOTypeCheckerTest, AllocReturnsPtr) {
    check(R"(
        func main() {
            var buf = alloc(100);
            unsafe { var next = buf + 10; }
            dealloc(buf);
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class BinaryIOCodegenTest : public ::testing::Test {
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

TEST_F(BinaryIOCodegenTest, FopenCallsRuntime) {
    auto ir = getIR(R"(func main() { var f = fopen("test.bin", "rb"); })");
    EXPECT_NE(ir.find("chris_fopen"), std::string::npos);
}

TEST_F(BinaryIOCodegenTest, AllocCallsRuntime) {
    auto ir = getIR(R"(func main() { var p = alloc(1024); })");
    EXPECT_NE(ir.find("chris_alloc"), std::string::npos);
}

TEST_F(BinaryIOCodegenTest, DeallocCallsRuntime) {
    auto ir = getIR(R"(func main() { var p = alloc(64); dealloc(p); })");
    EXPECT_NE(ir.find("chris_free"), std::string::npos);
}

TEST_F(BinaryIOCodegenTest, FreadCallsRuntime) {
    auto ir = getIR(R"(
        func test(handle: Int, buf: Ptr, n: Int) -> Int {
            return fread(handle, buf, n);
        }
    )");
    EXPECT_NE(ir.find("chris_fread"), std::string::npos);
}

TEST_F(BinaryIOCodegenTest, FsizeCallsRuntime) {
    auto ir = getIR(R"(func test(handle: Int) -> Int { return fsize(handle); })");
    EXPECT_NE(ir.find("chris_fsize"), std::string::npos);
}
