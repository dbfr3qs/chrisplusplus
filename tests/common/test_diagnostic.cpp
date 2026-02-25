#include <gtest/gtest.h>
#include "common/diagnostic.h"

using namespace chris;

TEST(DiagnosticTest, ErrorToString) {
    Diagnostic diag{
        "E0001",
        DiagnosticSeverity::Error,
        "unexpected token",
        SourceLocation("test.chr", 10, 5),
        "var x = @;",
        "Remove the invalid character"
    };

    std::string result = diag.toString();
    EXPECT_NE(result.find("error[E0001]"), std::string::npos);
    EXPECT_NE(result.find("unexpected token"), std::string::npos);
    EXPECT_NE(result.find("test.chr:10:5"), std::string::npos);
    EXPECT_NE(result.find("var x = @;"), std::string::npos);
    EXPECT_NE(result.find("Remove the invalid character"), std::string::npos);
}

TEST(DiagnosticTest, WarningToString) {
    Diagnostic diag{
        "W0001",
        DiagnosticSeverity::Warning,
        "unused variable",
        SourceLocation("test.chr", 3, 9),
        "var unused = 42;",
        std::nullopt
    };

    std::string result = diag.toString();
    EXPECT_NE(result.find("warning[W0001]"), std::string::npos);
    EXPECT_NE(result.find("unused variable"), std::string::npos);
}

TEST(DiagnosticTest, ToJson) {
    Diagnostic diag{
        "E0042",
        DiagnosticSeverity::Error,
        "Type 'String' is not assignable to 'Int'",
        SourceLocation("src/main.chr", 12, 5),
        "var x: Int = \"hello\";",
        "Change type to 'String' or convert the value"
    };

    std::string json = diag.toJson();
    EXPECT_NE(json.find("\"code\":\"E0042\""), std::string::npos);
    EXPECT_NE(json.find("\"severity\":\"error\""), std::string::npos);
    EXPECT_NE(json.find("\"line\":12"), std::string::npos);
    EXPECT_NE(json.find("\"column\":5"), std::string::npos);
    EXPECT_NE(json.find("\"suggestion\""), std::string::npos);
}

TEST(DiagnosticTest, ToJsonWithoutSuggestion) {
    Diagnostic diag{
        "E0001",
        DiagnosticSeverity::Error,
        "syntax error",
        SourceLocation("test.chr", 1, 1),
        "",
        std::nullopt
    };

    std::string json = diag.toJson();
    EXPECT_EQ(json.find("\"suggestion\""), std::string::npos);
}

TEST(DiagnosticEngineTest, ReportAndCount) {
    DiagnosticEngine engine;
    EXPECT_FALSE(engine.hasErrors());
    EXPECT_EQ(engine.errorCount(), 0u);
    EXPECT_EQ(engine.warningCount(), 0u);

    engine.error("E0001", "test error", SourceLocation("test.chr", 1, 1));
    EXPECT_TRUE(engine.hasErrors());
    EXPECT_EQ(engine.errorCount(), 1u);

    engine.warning("W0001", "test warning", SourceLocation("test.chr", 2, 1));
    EXPECT_EQ(engine.warningCount(), 1u);

    EXPECT_EQ(engine.diagnostics().size(), 2u);
}

TEST(DiagnosticEngineTest, Clear) {
    DiagnosticEngine engine;
    engine.error("E0001", "test error", SourceLocation("test.chr", 1, 1));
    EXPECT_TRUE(engine.hasErrors());

    engine.clear();
    EXPECT_FALSE(engine.hasErrors());
    EXPECT_EQ(engine.errorCount(), 0u);
    EXPECT_EQ(engine.diagnostics().size(), 0u);
}
