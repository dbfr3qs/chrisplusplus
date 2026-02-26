#include <gtest/gtest.h>
#include "codegen/codegen.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "sema/type_checker.h"
#include "common/diagnostic.h"

using namespace chris;

// ============================================================================
// Parser Tests for shared classes
// ============================================================================

class SharedParserTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parse(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        return parser.parse();
    }
};

TEST_F(SharedParserTest, SharedClassDecl) {
    auto program = parse(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->name, "Counter");
    EXPECT_TRUE(cls->isShared);
}

TEST_F(SharedParserTest, PublicSharedClassDecl) {
    auto program = parse(
        "public shared class SafeMap {\n"
        "    public var size: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    ASSERT_EQ(program.declarations.size(), 1u);

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_EQ(cls->name, "SafeMap");
    EXPECT_TRUE(cls->isShared);
    EXPECT_TRUE(cls->isPublic);
}

TEST_F(SharedParserTest, SharedClassWithMethods) {
    auto program = parse(
        "shared class Counter {\n"
        "    private var count: Int;\n"
        "    public func increment() {\n"
        "        this.count = this.count + 1;\n"
        "    }\n"
        "    public func get() -> Int {\n"
        "        return this.count;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_TRUE(cls->isShared);
    EXPECT_EQ(cls->fields.size(), 1u);
    EXPECT_EQ(cls->methods.size(), 2u);
}

TEST_F(SharedParserTest, NonSharedClassNotMarked) {
    auto program = parse(
        "class RegularClass {\n"
        "    public var value: Int;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());

    auto* cls = dynamic_cast<ClassDecl*>(program.declarations[0].get());
    ASSERT_NE(cls, nullptr);
    EXPECT_FALSE(cls->isShared);
}

// ============================================================================
// Type Checker Tests for shared classes
// ============================================================================

class SharedTypeCheckerTest : public ::testing::Test {
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

TEST_F(SharedTypeCheckerTest, SharedClassValid) {
    parseAndCheck(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(SharedTypeCheckerTest, SharedClassWithConstructor) {
    parseAndCheck(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "    func new() -> Counter {\n"
        "        return Counter { count: 0 };\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(SharedTypeCheckerTest, SharedClassMethodAccess) {
    parseAndCheck(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "    public func get() -> Int {\n"
        "        return this.count;\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// CodeGen Tests for shared classes
// ============================================================================

class SharedCodeGenTest : public ::testing::Test {
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

TEST_F(SharedCodeGenTest, SharedClassHasMutexField) {
    auto ir = generateIR(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "}\n"
        "func main() {\n"
        "    var c = Counter { count: 0 };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // Shared class struct should have a [64 x i8] mutex field followed by the i64 count field
    EXPECT_NE(ir.find("%Counter = type { [64 x i8], i64 }"), std::string::npos);
}

TEST_F(SharedCodeGenTest, SharedClassConstructCallsMutexInit) {
    auto ir = generateIR(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "}\n"
        "func main() {\n"
        "    var c = Counter { count: 0 };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_mutex_init"), std::string::npos);
}

TEST_F(SharedCodeGenTest, SharedClassFieldReadCallsLockUnlock) {
    auto ir = generateIR(
        "shared class Counter {\n"
        "    public var count: Int;\n"
        "    public func get() -> Int {\n"
        "        return this.count;\n"
        "    }\n"
        "}\n"
        "func main() { }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_mutex_lock"), std::string::npos);
    EXPECT_NE(ir.find("chris_mutex_unlock"), std::string::npos);
}

TEST_F(SharedCodeGenTest, NonSharedClassNoMutex) {
    auto ir = generateIR(
        "class Regular {\n"
        "    public var value: Int;\n"
        "}\n"
        "func main() { }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // Non-shared class should NOT have the 64-byte mutex array
    EXPECT_EQ(ir.find("Regular = type { [64 x i8]"), std::string::npos);
    EXPECT_EQ(ir.find("Regular = type { i64 }"), std::string::npos); // Regular = type { i64 } is fine
}

TEST_F(SharedCodeGenTest, MutexRuntimeFunctionsDeclared) {
    auto ir = generateIR("func main() { }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("declare void @chris_mutex_init"), std::string::npos);
    EXPECT_NE(ir.find("declare void @chris_mutex_lock"), std::string::npos);
    EXPECT_NE(ir.find("declare void @chris_mutex_unlock"), std::string::npos);
    EXPECT_NE(ir.find("declare void @chris_mutex_destroy"), std::string::npos);
}
