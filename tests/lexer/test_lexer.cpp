#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "common/diagnostic.h"

using namespace chris;

class LexerTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    std::vector<Token> lex(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        return lexer.tokenize();
    }

    // Helper: lex and return non-comment, non-EOF tokens
    std::vector<Token> lexCode(const std::string& source) {
        auto tokens = lex(source);
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

// --- Integer Literals ---

TEST_F(LexerTest, IntegerLiteral) {
    auto tokens = lexCode("42");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::IntLiteral);
    EXPECT_EQ(tokens[0].value, "42");
}

TEST_F(LexerTest, IntegerWithUnderscores) {
    auto tokens = lexCode("1_000_000");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::IntLiteral);
    EXPECT_EQ(tokens[0].value, "1000000");
}

TEST_F(LexerTest, Zero) {
    auto tokens = lexCode("0");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::IntLiteral);
    EXPECT_EQ(tokens[0].value, "0");
}

// --- Float Literals ---

TEST_F(LexerTest, FloatLiteral) {
    auto tokens = lexCode("3.14");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::FloatLiteral);
    EXPECT_EQ(tokens[0].value, "3.14");
}

TEST_F(LexerTest, FloatWithUnderscores) {
    auto tokens = lexCode("1_000.50");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::FloatLiteral);
    EXPECT_EQ(tokens[0].value, "1000.50");
}

// --- String Literals ---

TEST_F(LexerTest, SimpleString) {
    auto tokens = lexCode("\"hello\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "hello");
}

TEST_F(LexerTest, StringWithEscapes) {
    auto tokens = lexCode("\"hello\\nworld\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "hello\nworld");
}

TEST_F(LexerTest, EmptyString) {
    auto tokens = lexCode("\"\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "");
}

TEST_F(LexerTest, StringWithEscapedQuote) {
    auto tokens = lexCode("\"say \\\"hi\\\"\"");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[0].value, "say \"hi\"");
}

// --- String Interpolation ---

TEST_F(LexerTest, StringInterpolation) {
    auto tokens = lexCode("\"hello ${name}!\"");
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::StringInterpStart);
    EXPECT_EQ(tokens[0].value, "hello ");
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "name");
    EXPECT_EQ(tokens[2].type, TokenType::StringInterpEnd);
    EXPECT_EQ(tokens[2].value, "!");
}

TEST_F(LexerTest, StringInterpolationMultiple) {
    auto tokens = lexCode("\"${a} and ${b}\"");
    ASSERT_GE(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokenType::StringInterpStart);
    EXPECT_EQ(tokens[0].value, "");
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "a");
    EXPECT_EQ(tokens[2].type, TokenType::StringInterpMiddle);
    EXPECT_EQ(tokens[2].value, " and ");
    EXPECT_EQ(tokens[3].type, TokenType::Identifier);
    EXPECT_EQ(tokens[3].value, "b");
    EXPECT_EQ(tokens[4].type, TokenType::StringInterpEnd);
    EXPECT_EQ(tokens[4].value, "");
}

// --- Char Literals ---

TEST_F(LexerTest, CharLiteral) {
    auto tokens = lexCode("'a'");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::CharLiteral);
    EXPECT_EQ(tokens[0].value, "a");
}

TEST_F(LexerTest, CharLiteralEscape) {
    auto tokens = lexCode("'\\n'");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::CharLiteral);
    EXPECT_EQ(tokens[0].value, "\n");
}

// --- Bool and Nil Literals ---

TEST_F(LexerTest, BoolLiterals) {
    auto tokens = lexCode("true false");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::BoolLiteral);
    EXPECT_EQ(tokens[0].value, "true");
    EXPECT_EQ(tokens[1].type, TokenType::BoolLiteral);
    EXPECT_EQ(tokens[1].value, "false");
}

TEST_F(LexerTest, NilLiteral) {
    auto tokens = lexCode("nil");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::NilLiteral);
}

// --- Keywords ---

TEST_F(LexerTest, AllKeywords) {
    auto tokens = lexCode(
        "func var let class interface enum if else for while return "
        "import package public private protected throw try catch finally "
        "async await io compute unsafe shared new match operator extern in break continue"
    );
    EXPECT_EQ(tokens[0].type, TokenType::KwFunc);
    EXPECT_EQ(tokens[1].type, TokenType::KwVar);
    EXPECT_EQ(tokens[2].type, TokenType::KwLet);
    EXPECT_EQ(tokens[3].type, TokenType::KwClass);
    EXPECT_EQ(tokens[4].type, TokenType::KwInterface);
    EXPECT_EQ(tokens[5].type, TokenType::KwEnum);
    EXPECT_EQ(tokens[6].type, TokenType::KwIf);
    EXPECT_EQ(tokens[7].type, TokenType::KwElse);
    EXPECT_EQ(tokens[8].type, TokenType::KwFor);
    EXPECT_EQ(tokens[9].type, TokenType::KwWhile);
    EXPECT_EQ(tokens[10].type, TokenType::KwReturn);
    EXPECT_EQ(tokens[11].type, TokenType::KwImport);
    EXPECT_EQ(tokens[12].type, TokenType::KwPackage);
    EXPECT_EQ(tokens[13].type, TokenType::KwPublic);
    EXPECT_EQ(tokens[14].type, TokenType::KwPrivate);
    EXPECT_EQ(tokens[15].type, TokenType::KwProtected);
    EXPECT_EQ(tokens[16].type, TokenType::KwThrow);
    EXPECT_EQ(tokens[17].type, TokenType::KwTry);
    EXPECT_EQ(tokens[18].type, TokenType::KwCatch);
    EXPECT_EQ(tokens[19].type, TokenType::KwFinally);
    EXPECT_EQ(tokens[20].type, TokenType::KwAsync);
    EXPECT_EQ(tokens[21].type, TokenType::KwAwait);
    EXPECT_EQ(tokens[22].type, TokenType::KwIo);
    EXPECT_EQ(tokens[23].type, TokenType::KwCompute);
    EXPECT_EQ(tokens[24].type, TokenType::KwUnsafe);
    EXPECT_EQ(tokens[25].type, TokenType::KwShared);
    EXPECT_EQ(tokens[26].type, TokenType::KwNew);
    EXPECT_EQ(tokens[27].type, TokenType::KwMatch);
    EXPECT_EQ(tokens[28].type, TokenType::KwOperator);
    EXPECT_EQ(tokens[29].type, TokenType::KwExtern);
    EXPECT_EQ(tokens[30].type, TokenType::KwIn);
    EXPECT_EQ(tokens[31].type, TokenType::KwBreak);
    EXPECT_EQ(tokens[32].type, TokenType::KwContinue);
}

// --- Identifiers ---

TEST_F(LexerTest, Identifier) {
    auto tokens = lexCode("myVar");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::Identifier);
    EXPECT_EQ(tokens[0].value, "myVar");
}

TEST_F(LexerTest, IdentifierWithUnderscore) {
    auto tokens = lexCode("_private my_var __init");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].value, "_private");
    EXPECT_EQ(tokens[1].value, "my_var");
    EXPECT_EQ(tokens[2].value, "__init");
}

// --- Operators ---

TEST_F(LexerTest, ArithmeticOperators) {
    auto tokens = lexCode("+ - * / %");
    ASSERT_EQ(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokenType::Plus);
    EXPECT_EQ(tokens[1].type, TokenType::Minus);
    EXPECT_EQ(tokens[2].type, TokenType::Star);
    EXPECT_EQ(tokens[3].type, TokenType::Slash);
    EXPECT_EQ(tokens[4].type, TokenType::Percent);
}

TEST_F(LexerTest, ComparisonOperators) {
    auto tokens = lexCode("== != < > <= >=");
    ASSERT_EQ(tokens.size(), 6u);
    EXPECT_EQ(tokens[0].type, TokenType::Equal);
    EXPECT_EQ(tokens[1].type, TokenType::NotEqual);
    EXPECT_EQ(tokens[2].type, TokenType::Less);
    EXPECT_EQ(tokens[3].type, TokenType::Greater);
    EXPECT_EQ(tokens[4].type, TokenType::LessEqual);
    EXPECT_EQ(tokens[5].type, TokenType::GreaterEqual);
}

TEST_F(LexerTest, LogicalOperators) {
    auto tokens = lexCode("&& || !");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::And);
    EXPECT_EQ(tokens[1].type, TokenType::Or);
    EXPECT_EQ(tokens[2].type, TokenType::Not);
}

TEST_F(LexerTest, ArrowOperators) {
    auto tokens = lexCode("-> =>");
    ASSERT_EQ(tokens.size(), 2u);
    EXPECT_EQ(tokens[0].type, TokenType::Arrow);
    EXPECT_EQ(tokens[1].type, TokenType::FatArrow);
}

TEST_F(LexerTest, NullOperators) {
    auto tokens = lexCode("? ?. ??");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::QuestionMark);
    EXPECT_EQ(tokens[1].type, TokenType::QuestionDot);
    EXPECT_EQ(tokens[2].type, TokenType::DoubleQuestion);
}

TEST_F(LexerTest, DotOperators) {
    auto tokens = lexCode(". .. ...");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::Dot);
    EXPECT_EQ(tokens[1].type, TokenType::DotDot);
    EXPECT_EQ(tokens[2].type, TokenType::Ellipsis);
}

TEST_F(LexerTest, Assignment) {
    auto tokens = lexCode("=");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::Assign);
}

// --- Punctuation ---

TEST_F(LexerTest, Punctuation) {
    auto tokens = lexCode("( ) { } [ ] ; : , . @");
    ASSERT_EQ(tokens.size(), 11u);
    EXPECT_EQ(tokens[0].type, TokenType::LeftParen);
    EXPECT_EQ(tokens[1].type, TokenType::RightParen);
    EXPECT_EQ(tokens[2].type, TokenType::LeftBrace);
    EXPECT_EQ(tokens[3].type, TokenType::RightBrace);
    EXPECT_EQ(tokens[4].type, TokenType::LeftBracket);
    EXPECT_EQ(tokens[5].type, TokenType::RightBracket);
    EXPECT_EQ(tokens[6].type, TokenType::Semicolon);
    EXPECT_EQ(tokens[7].type, TokenType::Colon);
    EXPECT_EQ(tokens[8].type, TokenType::Comma);
    EXPECT_EQ(tokens[9].type, TokenType::Dot);
    EXPECT_EQ(tokens[10].type, TokenType::At);
}

// --- Comments ---

TEST_F(LexerTest, LineComment) {
    auto tokens = lex("// this is a comment\n42");
    // Should have: LineComment, IntLiteral, EOF
    bool hasComment = false;
    bool hasInt = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::LineComment) hasComment = true;
        if (t.type == TokenType::IntLiteral) hasInt = true;
    }
    EXPECT_TRUE(hasComment);
    EXPECT_TRUE(hasInt);
}

TEST_F(LexerTest, BlockComment) {
    auto tokens = lex("/* block */ 42");
    bool hasComment = false;
    bool hasInt = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::BlockComment) hasComment = true;
        if (t.type == TokenType::IntLiteral) hasInt = true;
    }
    EXPECT_TRUE(hasComment);
    EXPECT_TRUE(hasInt);
}

TEST_F(LexerTest, DocComment) {
    auto tokens = lex("/// This is a doc comment\n42");
    bool hasDoc = false;
    bool hasInt = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DocComment) {
            hasDoc = true;
            EXPECT_EQ(t.value, "This is a doc comment");
        }
        if (t.type == TokenType::IntLiteral) hasInt = true;
    }
    EXPECT_TRUE(hasDoc);
    EXPECT_TRUE(hasInt);
}

TEST_F(LexerTest, NestedBlockComment) {
    auto tokens = lex("/* outer /* inner */ still outer */ 42");
    bool hasComment = false;
    bool hasInt = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::BlockComment) hasComment = true;
        if (t.type == TokenType::IntLiteral) hasInt = true;
    }
    EXPECT_TRUE(hasComment);
    EXPECT_TRUE(hasInt);
}

// --- Source Locations ---

TEST_F(LexerTest, SourceLocations) {
    auto tokens = lexCode("var x = 42;");
    ASSERT_GE(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].location.line, 1u);
    EXPECT_EQ(tokens[0].location.column, 1u);
    EXPECT_EQ(tokens[0].location.file, "test.chr");
}

TEST_F(LexerTest, MultiLineLocations) {
    auto tokens = lexCode("var x\nvar y");
    ASSERT_GE(tokens.size(), 4u);
    EXPECT_EQ(tokens[0].location.line, 1u); // var
    EXPECT_EQ(tokens[2].location.line, 2u); // second var
}

// --- Error Recovery ---

TEST_F(LexerTest, InvalidCharacterRecovery) {
    auto tokens = lexCode("var ~ x");
    // Should produce tokens for var, error for ~, and identifier x
    bool hasError = false;
    bool hasVar = false;
    bool hasIdent = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::Error) hasError = true;
        if (t.type == TokenType::KwVar) hasVar = true;
        if (t.type == TokenType::Identifier) hasIdent = true;
    }
    EXPECT_TRUE(hasError);
    EXPECT_TRUE(hasVar);
    EXPECT_TRUE(hasIdent);
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(LexerTest, UnterminatedString) {
    auto tokens = lex("\"hello");
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(LexerTest, UnterminatedBlockComment) {
    auto tokens = lex("/* unclosed");
    EXPECT_TRUE(diag.hasErrors());
}

// --- Complete Program ---

TEST_F(LexerTest, HelloWorldProgram) {
    auto tokens = lexCode(
        "func main() {\n"
        "    var name = \"Chris\";\n"
        "    print(\"Hello, ${name}!\");\n"
        "}\n"
    );

    // func main ( ) { var name = "Chris" ; print ( StringInterpStart Ident StringInterpEnd ) ; }
    ASSERT_GE(tokens.size(), 10u);
    EXPECT_EQ(tokens[0].type, TokenType::KwFunc);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "main");
    EXPECT_EQ(tokens[2].type, TokenType::LeftParen);
    EXPECT_EQ(tokens[3].type, TokenType::RightParen);
    EXPECT_EQ(tokens[4].type, TokenType::LeftBrace);
    EXPECT_EQ(tokens[5].type, TokenType::KwVar);
    EXPECT_EQ(tokens[6].type, TokenType::Identifier);
    EXPECT_EQ(tokens[6].value, "name");
    EXPECT_EQ(tokens[7].type, TokenType::Assign);
    EXPECT_EQ(tokens[8].type, TokenType::StringLiteral);
    EXPECT_EQ(tokens[8].value, "Chris");
    EXPECT_EQ(tokens[9].type, TokenType::Semicolon);
}

TEST_F(LexerTest, FunctionWithTypes) {
    auto tokens = lexCode("func add(a: Int, b: Int) -> Int { return a + b; }");
    EXPECT_EQ(tokens[0].type, TokenType::KwFunc);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].value, "add");
    EXPECT_EQ(tokens[2].type, TokenType::LeftParen);
    EXPECT_EQ(tokens[3].type, TokenType::Identifier); // a
    EXPECT_EQ(tokens[4].type, TokenType::Colon);
    EXPECT_EQ(tokens[5].type, TokenType::Identifier); // Int
    EXPECT_EQ(tokens[6].type, TokenType::Comma);
    EXPECT_EQ(tokens[10].type, TokenType::RightParen);
    EXPECT_EQ(tokens[11].type, TokenType::Arrow);
    EXPECT_EQ(tokens[12].type, TokenType::Identifier); // Int
}

TEST_F(LexerTest, NullSafetySyntax) {
    auto tokens = lexCode("var x: Int? = nil; var y = x ?? 0; var z = x?.toString();");
    bool hasQuestion = false;
    bool hasDoubleQuestion = false;
    bool hasQuestionDot = false;
    bool hasNil = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::QuestionMark) hasQuestion = true;
        if (t.type == TokenType::DoubleQuestion) hasDoubleQuestion = true;
        if (t.type == TokenType::QuestionDot) hasQuestionDot = true;
        if (t.type == TokenType::NilLiteral) hasNil = true;
    }
    EXPECT_TRUE(hasQuestion);
    EXPECT_TRUE(hasDoubleQuestion);
    EXPECT_TRUE(hasQuestionDot);
    EXPECT_TRUE(hasNil);
}

TEST_F(LexerTest, LambdaSyntax) {
    auto tokens = lexCode("var f = (x) => x * 2;");
    bool hasFatArrow = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::FatArrow) hasFatArrow = true;
    }
    EXPECT_TRUE(hasFatArrow);
}

TEST_F(LexerTest, RangeSyntax) {
    auto tokens = lexCode("for i in 0..10 { }");
    bool hasDotDot = false;
    bool hasIn = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::DotDot) hasDotDot = true;
        if (t.type == TokenType::KwIn) hasIn = true;
    }
    EXPECT_TRUE(hasDotDot);
    EXPECT_TRUE(hasIn);
}

// --- EOF ---

TEST_F(LexerTest, EmptySource) {
    auto tokens = lex("");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::EndOfFile);
}

TEST_F(LexerTest, WhitespaceOnly) {
    auto tokens = lex("   \n\t\n  ");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, TokenType::EndOfFile);
}

// --- Range does not conflict with float ---

TEST_F(LexerTest, RangeVsFloat) {
    // 0..10 should be Int(0), DotDot, Int(10) — not Float(0.) followed by something
    auto tokens = lexCode("0..10");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::IntLiteral);
    EXPECT_EQ(tokens[0].value, "0");
    EXPECT_EQ(tokens[1].type, TokenType::DotDot);
    EXPECT_EQ(tokens[2].type, TokenType::IntLiteral);
    EXPECT_EQ(tokens[2].value, "10");
}
