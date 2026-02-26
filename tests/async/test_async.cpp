#include <gtest/gtest.h>
#include "codegen/codegen.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "sema/type_checker.h"
#include "common/diagnostic.h"

using namespace chris;

// ============================================================================
// Parser Tests for async/await
// ============================================================================

class AsyncParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

TEST_F(AsyncParserTest, AsyncFuncDecl) {
    auto program = parse(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_EQ(func->name, "fetchData");
    EXPECT_TRUE(func->isAsync);
    EXPECT_EQ(func->asyncKind, AsyncKind::None);
}

TEST_F(AsyncParserTest, AsyncFuncWithIoAnnotation) {
    auto program = parse(
        "async func readFile(path: String) -> io String {\n"
        "    return \"content\";\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_TRUE(func->isAsync);
    EXPECT_EQ(func->asyncKind, AsyncKind::Io);
}

TEST_F(AsyncParserTest, AsyncFuncWithComputeAnnotation) {
    auto program = parse(
        "async func heavyCalc(n: Int) -> compute Int {\n"
        "    return n * n;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_TRUE(func->isAsync);
    EXPECT_EQ(func->asyncKind, AsyncKind::Compute);
}

TEST_F(AsyncParserTest, AwaitExpression) {
    auto program = parse(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "async func main() {\n"
        "    let result = await fetchData();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 2u);
}

TEST_F(AsyncParserTest, AsyncFuncNoReturnType) {
    auto program = parse(
        "async func doWork() {\n"
        "    let x = 42;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* func = dynamic_cast<FuncDecl*>(program.declarations[0].get());
    ASSERT_NE(func, nullptr);
    EXPECT_TRUE(func->isAsync);
    EXPECT_EQ(func->returnType, nullptr);
}

TEST_F(AsyncParserTest, MultipleAwaitExpressions) {
    auto program = parse(
        "async func a() -> Int { return 1; }\n"
        "async func b() -> Int { return 2; }\n"
        "async func combined() -> Int {\n"
        "    let x = await a();\n"
        "    let y = await b();\n"
        "    return x + y;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 3u);
}

// ============================================================================
// Type Checker Tests for async/await
// ============================================================================

class AsyncTypeCheckerTest : public ::testing::Test {
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

    bool findError(const std::string& code) {
        for (const auto& d : diag.diagnostics()) {
            if (d.code == code) return true;
        }
        return false;
    }
};

TEST_F(AsyncTypeCheckerTest, AsyncFuncValid) {
    parseAndCheck(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(AsyncTypeCheckerTest, AwaitInsideAsyncFunc) {
    parseAndCheck(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "async func main() {\n"
        "    let result = await fetchData();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(AsyncTypeCheckerTest, AwaitOutsideAsyncFuncErrors) {
    parseAndCheck(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "func main() {\n"
        "    let result = await fetchData();\n"
        "}\n"
    );
    EXPECT_TRUE(findError("E3031"));
}

TEST_F(AsyncTypeCheckerTest, IoComputeWithoutAsyncErrors) {
    parseAndCheck(
        "func readFile() -> io String {\n"
        "    return \"data\";\n"
        "}\n"
    );
    EXPECT_TRUE(findError("E3030"));
}

TEST_F(AsyncTypeCheckerTest, AsyncIoFuncValid) {
    parseAndCheck(
        "async func readFile() -> io String {\n"
        "    return \"data\";\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(AsyncTypeCheckerTest, AsyncComputeFuncValid) {
    parseAndCheck(
        "async func heavyCalc(n: Int) -> compute Int {\n"
        "    return n * n;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// CodeGen Tests for async/await
// ============================================================================

class AsyncCodeGenTest : public ::testing::Test {
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

TEST_F(AsyncCodeGenTest, AsyncFuncReturnsPtr) {
    auto ir = generateIR(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "func main() { }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // Async function should return a pointer (Future*)
    EXPECT_NE(ir.find("define ptr @fetchData()"), std::string::npos);
}

TEST_F(AsyncCodeGenTest, AsyncFuncCallsSpawn) {
    auto ir = generateIR(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "func main() { }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_async_spawn"), std::string::npos);
}

TEST_F(AsyncCodeGenTest, AsyncFuncCreatesThunk) {
    auto ir = generateIR(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "func main() { }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("__async_fetchData"), std::string::npos);
}

TEST_F(AsyncCodeGenTest, AwaitCallsAsyncAwait) {
    auto ir = generateIR(
        "async func fetchData() -> Int {\n"
        "    return 42;\n"
        "}\n"
        "async func main() {\n"
        "    let result = await fetchData();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_async_await"), std::string::npos);
}

TEST_F(AsyncCodeGenTest, AsyncFuncWithParams) {
    auto ir = generateIR(
        "async func add(a: Int, b: Int) -> Int {\n"
        "    return a + b;\n"
        "}\n"
        "func main() { }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("define ptr @add(i64"), std::string::npos);
    EXPECT_NE(ir.find("chris_async_spawn"), std::string::npos);
}

TEST_F(AsyncCodeGenTest, RuntimeFunctionsDeclared) {
    auto ir = generateIR("func main() { }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("declare ptr @chris_async_spawn"), std::string::npos);
    EXPECT_NE(ir.find("declare i64 @chris_async_await"), std::string::npos);
    EXPECT_NE(ir.find("declare void @chris_async_run_loop"), std::string::npos);
}
