#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Type Checker Tests ====================

class TypedArraysTypeCheckerTest : public ::testing::Test {
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

TEST_F(TypedArraysTypeCheckerTest, PtrLoadInt32) {
    check("func test(p: Ptr) -> Int { return ptrLoadInt32(p); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypedArraysTypeCheckerTest, PtrStoreInt32) {
    check("func test(p: Ptr, v: Int) { ptrStoreInt32(p, v); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypedArraysTypeCheckerTest, PtrLoadInt16) {
    check("func test(p: Ptr) -> Int { return ptrLoadInt16(p); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypedArraysTypeCheckerTest, PtrStoreInt16) {
    check("func test(p: Ptr, v: Int) { ptrStoreInt16(p, v); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypedArraysTypeCheckerTest, Memcpy) {
    check("func test(dst: Ptr, src: Ptr, n: Int) { memcpy(dst, src, n); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypedArraysTypeCheckerTest, Memset) {
    check("func test(dst: Ptr, v: Int, n: Int) { memset(dst, v, n); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypedArraysTypeCheckerTest, PixelBufferPattern) {
    check(R"(
        func main() {
            var width = 320;
            var height = 240;
            var buf = alloc(width * height * 4);
            unsafe {
                var pixel = buf + 0;
                ptrStoreInt32(pixel, 255);
            }
            dealloc(buf);
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class TypedArraysCodegenTest : public ::testing::Test {
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

TEST_F(TypedArraysCodegenTest, PtrLoadInt32CallsRuntime) {
    auto ir = getIR("func test(p: Ptr) -> Int { return ptrLoadInt32(p); }");
    EXPECT_NE(ir.find("chris_ptr_load_i32"), std::string::npos);
}

TEST_F(TypedArraysCodegenTest, PtrStoreInt32CallsRuntime) {
    auto ir = getIR("func test(p: Ptr, v: Int) { ptrStoreInt32(p, v); }");
    EXPECT_NE(ir.find("chris_ptr_store_i32"), std::string::npos);
}

TEST_F(TypedArraysCodegenTest, MemcpyCallsRuntime) {
    auto ir = getIR("func test(d: Ptr, s: Ptr, n: Int) { memcpy(d, s, n); }");
    EXPECT_NE(ir.find("chris_memcpy"), std::string::npos);
}

TEST_F(TypedArraysCodegenTest, MemsetCallsRuntime) {
    auto ir = getIR("func test(d: Ptr, v: Int, n: Int) { memset(d, v, n); }");
    EXPECT_NE(ir.find("chris_memset"), std::string::npos);
}

TEST_F(TypedArraysCodegenTest, AllocPlusArithmeticPattern) {
    auto ir = getIR(R"(
        func main() {
            var buf = alloc(1024);
            unsafe {
                ptrStoreInt32(buf + 0, 42);
                ptrStoreInt32(buf + 4, 99);
            }
            dealloc(buf);
        }
    )");
    EXPECT_NE(ir.find("chris_alloc"), std::string::npos);
    EXPECT_NE(ir.find("chris_ptr_store_i32"), std::string::npos);
    EXPECT_NE(ir.find("getelementptr"), std::string::npos);
}
