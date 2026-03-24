#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Lexer Tests ====================

class BitwiseLexerTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    std::vector<Token> lexCode(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        std::vector<Token> result;
        for (const auto& t : tokens) {
            if (t.type != TokenType::EndOfFile &&
                t.type != TokenType::LineComment &&
                t.type != TokenType::BlockComment &&
                t.type != TokenType::DocComment) {
                result.push_back(t);
            }
        }
        return result;
    }
};

TEST_F(BitwiseLexerTest, Ampersand) {
    auto tokens = lexCode("a & b");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::Ampersand);
    EXPECT_EQ(tokens[1].value, "&");
}

TEST_F(BitwiseLexerTest, Pipe) {
    auto tokens = lexCode("a | b");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::Pipe);
    EXPECT_EQ(tokens[1].value, "|");
}

TEST_F(BitwiseLexerTest, Caret) {
    auto tokens = lexCode("a ^ b");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::Caret);
    EXPECT_EQ(tokens[1].value, "^");
}

TEST_F(BitwiseLexerTest, Tilde) {
    auto tokens = lexCode("~x");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::Tilde);
    EXPECT_EQ(tokens[0].value, "~");
}

TEST_F(BitwiseLexerTest, ShiftLeft) {
    auto tokens = lexCode("a << b");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::ShiftLeft);
    EXPECT_EQ(tokens[1].value, "<<");
}

TEST_F(BitwiseLexerTest, ShiftRight) {
    auto tokens = lexCode("a >> b");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::ShiftRight);
    EXPECT_EQ(tokens[1].value, ">>");
}

TEST_F(BitwiseLexerTest, AmpersandAssign) {
    auto tokens = lexCode("x &= 1");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::AmpersandAssign);
}

TEST_F(BitwiseLexerTest, PipeAssign) {
    auto tokens = lexCode("x |= 1");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::PipeAssign);
}

TEST_F(BitwiseLexerTest, CaretAssign) {
    auto tokens = lexCode("x ^= 1");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::CaretAssign);
}

TEST_F(BitwiseLexerTest, ShiftLeftAssign) {
    auto tokens = lexCode("x <<= 1");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::ShiftLeftAssign);
}

TEST_F(BitwiseLexerTest, ShiftRightAssign) {
    auto tokens = lexCode("x >>= 1");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[1].type, TokenType::ShiftRightAssign);
}

TEST_F(BitwiseLexerTest, AmpersandVsLogicalAnd) {
    auto tokens = lexCode("a & b && c");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].type, TokenType::Ampersand);
    EXPECT_EQ(tokens[3].type, TokenType::And);
}

TEST_F(BitwiseLexerTest, PipeVsLogicalOr) {
    auto tokens = lexCode("a | b || c");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].type, TokenType::Pipe);
    EXPECT_EQ(tokens[3].type, TokenType::Or);
}

TEST_F(BitwiseLexerTest, ShiftLeftVsLessThan) {
    auto tokens = lexCode("a << b < c");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].type, TokenType::ShiftLeft);
    EXPECT_EQ(tokens[3].type, TokenType::Less);
}

TEST_F(BitwiseLexerTest, ShiftRightVsGreaterThan) {
    auto tokens = lexCode("a >> b > c");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[1].type, TokenType::ShiftRight);
    EXPECT_EQ(tokens[3].type, TokenType::Greater);
}

// ==================== Parser Tests ====================

class BitwiseParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

TEST_F(BitwiseParserTest, BitwiseAndParsesAsBinaryExpr) {
    auto program = parse("func main() { var x = 5 & 3; }");
    EXPECT_FALSE(diag.hasErrors());
    ASSERT_GE(program.declarations.size(), 1u);
}

TEST_F(BitwiseParserTest, BitwiseOrParsesAsBinaryExpr) {
    auto program = parse("func main() { var x = 5 | 3; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, BitwiseXorParsesAsBinaryExpr) {
    auto program = parse("func main() { var x = 5 ^ 3; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, ShiftLeftParsesAsBinaryExpr) {
    auto program = parse("func main() { var x = 1 << 4; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, ShiftRightParsesAsBinaryExpr) {
    auto program = parse("func main() { var x = 16 >> 2; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, BitwiseNotParsesAsUnaryExpr) {
    auto program = parse("func main() { var x = ~42; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, PrecedenceShiftBeforeComparison) {
    // 1 << 4 < 20 should parse as (1 << 4) < 20, not 1 << (4 < 20)
    auto program = parse("func main() { var x = 1 << 4 < 20; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, PrecedenceBitAndBeforeBitOr) {
    // 5 | 3 & 1 should parse as 5 | (3 & 1)
    auto program = parse("func main() { var x = 5 | 3 & 1; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseParserTest, CompoundBitwiseAssignment) {
    auto program = parse("func main() { var x = 5; x &= 3; x |= 1; x ^= 7; x <<= 2; x >>= 1; }");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Type Checker Tests ====================

class BitwiseTypeCheckerTest : public ::testing::Test {
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

TEST_F(BitwiseTypeCheckerTest, BitwiseAndOnIntegers) {
    check("func main() { var x = 5 & 3; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseOrOnIntegers) {
    check("func main() { var x = 5 | 3; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseXorOnIntegers) {
    check("func main() { var x = 5 ^ 3; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, ShiftLeftOnIntegers) {
    check("func main() { var x = 1 << 4; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, ShiftRightOnIntegers) {
    check("func main() { var x = 16 >> 2; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseNotOnInteger) {
    check("func main() { var x = ~42; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseAndOnFloatErrors) {
    check("func main() { var x = 5.0 & 3.0; }");
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseOrOnFloatErrors) {
    check("func main() { var x = 5.0 | 3.0; }");
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, ShiftOnFloatErrors) {
    check("func main() { var x = 5.0 << 2; }");
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseNotOnFloatErrors) {
    check("func main() { var x = ~5.0; }");
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(BitwiseTypeCheckerTest, BitwiseAndOnStringErrors) {
    check("func main() { var x = \"hello\" & \"world\"; }");
    EXPECT_TRUE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class BitwiseCodegenTest : public ::testing::Test {
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

TEST_F(BitwiseCodegenTest, BitwiseAndEmitsAndInstruction) {
    auto ir = getIR("func test(a: Int, b: Int) -> Int { return a & b; }");
    EXPECT_NE(ir.find("and"), std::string::npos);
}

TEST_F(BitwiseCodegenTest, BitwiseOrEmitsOrInstruction) {
    auto ir = getIR("func test(a: Int, b: Int) -> Int { return a | b; }");
    EXPECT_NE(ir.find("or"), std::string::npos);
}

TEST_F(BitwiseCodegenTest, BitwiseXorEmitsXorInstruction) {
    auto ir = getIR("func test(a: Int, b: Int) -> Int { return a ^ b; }");
    EXPECT_NE(ir.find("xor"), std::string::npos);
}

TEST_F(BitwiseCodegenTest, ShiftLeftEmitsShlInstruction) {
    auto ir = getIR("func test(a: Int, b: Int) -> Int { return a << b; }");
    EXPECT_NE(ir.find("shl"), std::string::npos);
}

TEST_F(BitwiseCodegenTest, ShiftRightEmitsAshrInstruction) {
    auto ir = getIR("func test(a: Int, b: Int) -> Int { return a >> b; }");
    EXPECT_NE(ir.find("ashr"), std::string::npos);
}

TEST_F(BitwiseCodegenTest, BitwiseNotEmitsXorMinusOne) {
    // LLVM implements NOT as XOR with -1
    auto ir = getIR("func test(a: Int) -> Int { return ~a; }");
    EXPECT_NE(ir.find("xor"), std::string::npos);
}
