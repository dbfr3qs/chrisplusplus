#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Type Checker Tests ====================

class CastingTypeCheckerTest : public ::testing::Test {
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

TEST_F(CastingTypeCheckerTest, IntToChar) {
    check("func main() { var x = 65; var c = x.toChar(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, CharToInt) {
    check("func test(c: Char) -> Int { return c.toInt(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, IntToFloat) {
    check("func main() { var x = 42; var f = x.toFloat(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, FloatToInt) {
    check("func main() { var f = 3.14; var x = f.toInt(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, IntToByte) {
    check("func main() { var x = 255; var b = x.toByte(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, IntToInt32) {
    check("func main() { var x = 42; var i = x.toInt32(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, IntToPtr) {
    check("func main() { var x = 42; var p = x.toPtr(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, FloatToFloat32) {
    check("func main() { var f = 3.14; var f32 = f.toFloat32(); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, PtrToIntBuiltin) {
    check("func test(p: Ptr) -> Int { return ptrToInt(p); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, IntToPtrBuiltin) {
    check("func test(n: Int) -> Ptr { return intToPtr(n); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, PtrLoadBuiltin) {
    check("func test(p: Ptr) -> Int { return ptrLoad(p); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, PtrStoreBuiltin) {
    check("func test(p: Ptr, v: Int) { ptrStore(p, v); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, PtrLoadByteBuiltin) {
    check("func test(p: Ptr) -> Int { return ptrLoadByte(p); }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(CastingTypeCheckerTest, PtrStoreByteBuiltin) {
    check("func test(p: Ptr, v: Int) { ptrStoreByte(p, v); }");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class CastingCodegenTest : public ::testing::Test {
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

TEST_F(CastingCodegenTest, IntToCharEmitsTrunc) {
    auto ir = getIR("func test(x: Int) -> Char { return x.toChar(); }");
    EXPECT_NE(ir.find("trunc"), std::string::npos);
}

TEST_F(CastingCodegenTest, CharToIntEmitsSExt) {
    auto ir = getIR("func test(c: Char) -> Int { return c.toInt(); }");
    // sext or zext
    bool hasSExt = ir.find("sext") != std::string::npos;
    bool hasZExt = ir.find("zext") != std::string::npos;
    EXPECT_TRUE(hasSExt || hasZExt);
}

TEST_F(CastingCodegenTest, IntToFloatEmitsSIToFP) {
    auto ir = getIR("func test(x: Int) -> Float { return x.toFloat(); }");
    EXPECT_NE(ir.find("sitofp"), std::string::npos);
}

TEST_F(CastingCodegenTest, FloatToIntEmitsFPToSI) {
    auto ir = getIR("func test(f: Float) -> Int { return f.toInt(); }");
    EXPECT_NE(ir.find("fptosi"), std::string::npos);
}

TEST_F(CastingCodegenTest, IntToByteEmitsTrunc) {
    auto ir = getIR("func test(x: Int) -> Int8 { return x.toByte(); }");
    EXPECT_NE(ir.find("trunc"), std::string::npos);
}

TEST_F(CastingCodegenTest, IntToInt32EmitsTrunc) {
    auto ir = getIR("func test(x: Int) -> Int32 { return x.toInt32(); }");
    EXPECT_NE(ir.find("trunc"), std::string::npos);
}

TEST_F(CastingCodegenTest, FloatToFloat32EmitsFPTrunc) {
    auto ir = getIR("func test(f: Float) -> Float32 { return f.toFloat32(); }");
    EXPECT_NE(ir.find("fptrunc"), std::string::npos);
}

TEST_F(CastingCodegenTest, PtrToIntEmitsPtrToInt) {
    auto ir = getIR("func test(p: Ptr) -> Int { return ptrToInt(p); }");
    EXPECT_NE(ir.find("ptrtoint"), std::string::npos);
}

TEST_F(CastingCodegenTest, IntToPtrEmitsIntToPtr) {
    auto ir = getIR("func test(n: Int) -> Ptr { return intToPtr(n); }");
    EXPECT_NE(ir.find("inttoptr"), std::string::npos);
}

TEST_F(CastingCodegenTest, IntToPtrMethodEmitsIntToPtr) {
    auto ir = getIR("func test(n: Int) -> Ptr { return n.toPtr(); }");
    EXPECT_NE(ir.find("inttoptr"), std::string::npos);
}

TEST_F(CastingCodegenTest, PtrLoadEmitsLoad) {
    auto ir = getIR("func test(p: Ptr) -> Int { return ptrLoad(p); }");
    EXPECT_NE(ir.find("load"), std::string::npos);
}

TEST_F(CastingCodegenTest, PtrLoadByteEmitsI8Load) {
    auto ir = getIR("func test(p: Ptr) -> Int { return ptrLoadByte(p); }");
    EXPECT_NE(ir.find("load"), std::string::npos);
}
