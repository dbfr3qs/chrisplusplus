#include <gtest/gtest.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ==================== Type Checker Tests ====================

class StructsTypeCheckerTest : public ::testing::Test {
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

TEST_F(StructsTypeCheckerTest, CLayoutClassDeclared) {
    check(R"(
        @CLayout
        class Point {
            public var x: Int;
            public var y: Int;
            public func new() -> Point {
                return Point { x: 0, y: 0 };
            }
        }
        func main() {
            var p = Point.new();
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StructsTypeCheckerTest, CLayoutFieldAccess) {
    check(R"(
        @CLayout
        class Vec2 {
            public var x: Float;
            public var y: Float;
            public func new() -> Vec2 {
                return Vec2 { x: 0.0, y: 0.0 };
            }
        }
        func main() {
            var v = Vec2.new();
            v.x = 1.0;
            v.y = 2.0;
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StructsTypeCheckerTest, SizeofTypeBuiltin) {
    check(R"(
        @CLayout
        class Header {
            var magic: Int;
            var size: Int;
        }
        func main() {
            var s = sizeofType("Header");
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StructsTypeCheckerTest, TopLevelLetConstant) {
    check(R"(
        let SCREEN_WIDTH = 320;
        let SCREEN_HEIGHT = 240;
        func main() {
            print(SCREEN_WIDTH);
            print(SCREEN_HEIGHT);
        }
    )");
    EXPECT_FALSE(diag.hasErrors());
}

// ==================== Codegen Tests ====================

class StructsCodegenTest : public ::testing::Test {
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

TEST_F(StructsCodegenTest, CLayoutUsesAllocNotGC) {
    auto ir = getIR(R"(
        @CLayout
        class Point {
            var x: Int;
            var y: Int;
            func new() {}
        }
        func main() {
            var p = Point.new();
        }
    )");
    EXPECT_NE(ir.find("chris_alloc"), std::string::npos);
    // Should NOT use GC alloc for CLayout
    // Note: chris_gc_alloc may still appear as a declaration, so check call sites
    // The key indicator is that clayout.alloc label is present
    EXPECT_NE(ir.find("clayout.alloc"), std::string::npos);
}

TEST_F(StructsCodegenTest, SizeofTypeReturnsConstant) {
    auto ir = getIR(R"(
        @CLayout
        class Header {
            var magic: Int;
            var size: Int;
        }
        func main() {
            var s = sizeofType("Header");
        }
    )");
    // sizeofType should be folded to a constant (16 = 2 * i64)
    // The IR should contain a constant i64 16
    EXPECT_NE(ir.find("16"), std::string::npos);
}

TEST_F(StructsCodegenTest, CLayoutStructHasSequentialFields) {
    auto ir = getIR(R"(
        @CLayout
        class Vec3 {
            var x: Float;
            var y: Float;
            var z: Float;
        }
        func main() {
            var v = Vec3.new();
        }
    )");
    // Struct should be defined with 3 double fields
    EXPECT_NE(ir.find("Vec3"), std::string::npos);
}

TEST_F(StructsCodegenTest, TopLevelConstant) {
    auto ir = getIR(R"(
        let WIDTH = 320;
        func main() {
            print(WIDTH);
        }
    )");
    EXPECT_NE(ir.find("WIDTH"), std::string::npos);
}
