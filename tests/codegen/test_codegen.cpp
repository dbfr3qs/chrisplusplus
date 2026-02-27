#include <gtest/gtest.h>
#include "codegen/codegen.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "sema/type_checker.h"
#include "common/diagnostic.h"

using namespace chris;

class CodeGenTest : public ::testing::Test {
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

TEST_F(CodeGenTest, EmptyMain) {
    auto ir = generateIR("func main() { }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("define i32 @main()"), std::string::npos);
    EXPECT_NE(ir.find("ret i32 0"), std::string::npos);
}

TEST_F(CodeGenTest, IntVariable) {
    auto ir = generateIR("func main() { var x = 42; }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("alloca i64"), std::string::npos);
    EXPECT_NE(ir.find("store i64 42"), std::string::npos);
}

TEST_F(CodeGenTest, IntArithmetic) {
    auto ir = generateIR(
        "func main() {\n"
        "    var a = 2;\n"
        "    var b = 3;\n"
        "    var x = a + b * a;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("mul"), std::string::npos);
    EXPECT_NE(ir.find("add"), std::string::npos);
}

TEST_F(CodeGenTest, PrintStringLiteral) {
    auto ir = generateIR("func main() { print(\"hello\"); }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("call void @chris_print"), std::string::npos);
    EXPECT_NE(ir.find("hello"), std::string::npos);
}

TEST_F(CodeGenTest, StringInterpolation) {
    auto ir = generateIR(
        "func main() {\n"
        "    var name = \"Chris\";\n"
        "    print(\"Hello, ${name}!\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_strcat"), std::string::npos);
    EXPECT_NE(ir.find("chris_print"), std::string::npos);
}

TEST_F(CodeGenTest, IfStatement) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 10;\n"
        "    if x > 5 {\n"
        "        print(\"big\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("br i1"), std::string::npos);
    EXPECT_NE(ir.find("then"), std::string::npos);
}

TEST_F(CodeGenTest, IfElseStatement) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 10;\n"
        "    if x > 5 {\n"
        "        print(\"big\");\n"
        "    } else {\n"
        "        print(\"small\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("then"), std::string::npos);
    EXPECT_NE(ir.find("else"), std::string::npos);
}

TEST_F(CodeGenTest, WhileLoop) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 10;\n"
        "    while x > 0 {\n"
        "        x = x - 1;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("whilecond"), std::string::npos);
    EXPECT_NE(ir.find("whilebody"), std::string::npos);
}

TEST_F(CodeGenTest, ReturnValue) {
    auto ir = generateIR("func add(a: Int, b: Int) -> Int { return a + b; }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("define i64 @add(i64"), std::string::npos);
    EXPECT_NE(ir.find("ret i64"), std::string::npos);
}

TEST_F(CodeGenTest, FunctionCall) {
    auto ir = generateIR(
        "func add(a: Int, b: Int) -> Int { return a + b; }\n"
        "func main() { var x = add(1, 2); }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("call i64 @add(i64 1, i64 2)"), std::string::npos);
}

TEST_F(CodeGenTest, BoolLiteral) {
    auto ir = generateIR("func main() { var x = true; }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("alloca i1"), std::string::npos);
}

TEST_F(CodeGenTest, FloatLiteral) {
    auto ir = generateIR("func main() { var x = 3.14; }");
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("alloca double"), std::string::npos);
}

TEST_F(CodeGenTest, UnaryNegation) {
    auto ir = generateIR(
        "func main() {\n"
        "    var a = 42;\n"
        "    var x = -a;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // LLVM emits 'sub nsw i64 0, %a' for negation
    EXPECT_NE(ir.find("sub"), std::string::npos);
}

TEST_F(CodeGenTest, Comparison) {
    auto ir = generateIR(
        "func main() {\n"
        "    var a = 1;\n"
        "    var b = 2;\n"
        "    var x = a > b;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("icmp sgt"), std::string::npos);
}

TEST_F(CodeGenTest, IntToStringInInterpolation) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 42;\n"
        "    print(\"x is ${x}\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_int_to_str"), std::string::npos);
}

// --- Done Criterion ---

TEST_F(CodeGenTest, DoneCriterionIR) {
    auto ir = generateIR(
        "func main() {\n"
        "    var name = \"Chris\";\n"
        "    print(\"Hello, ${name}!\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for done criterion program";
    EXPECT_NE(ir.find("define i32 @main()"), std::string::npos);
    EXPECT_NE(ir.find("chris_print"), std::string::npos);
    EXPECT_NE(ir.find("chris_strcat"), std::string::npos);
    EXPECT_NE(ir.find("Chris"), std::string::npos);
    EXPECT_NE(ir.find("Hello, "), std::string::npos);
}

// --- Phase 5: Functions & Control Flow ---

TEST_F(CodeGenTest, Recursion) {
    auto ir = generateIR(
        "func fibonacci(n: Int) -> Int {\n"
        "    if n <= 1 {\n"
        "        return n;\n"
        "    }\n"
        "    return fibonacci(n - 1) + fibonacci(n - 2);\n"
        "}\n"
        "func main() { var x = fibonacci(10); }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("define i64 @fibonacci(i64"), std::string::npos);
    EXPECT_NE(ir.find("call i64 @fibonacci"), std::string::npos);
}

TEST_F(CodeGenTest, ForInRange) {
    auto ir = generateIR(
        "func main() {\n"
        "    for i in 0..10 {\n"
        "        print(\"hello\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("forcond"), std::string::npos);
    EXPECT_NE(ir.find("forbody"), std::string::npos);
    EXPECT_NE(ir.find("forinc"), std::string::npos);
    EXPECT_NE(ir.find("icmp slt"), std::string::npos);
}

TEST_F(CodeGenTest, BreakInWhile) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 10;\n"
        "    while x > 0 {\n"
        "        if x == 5 { break; }\n"
        "        x = x - 1;\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("whileend"), std::string::npos);
}

TEST_F(CodeGenTest, ContinueInWhile) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 10;\n"
        "    while x > 0 {\n"
        "        x = x - 1;\n"
        "        if x == 5 { continue; }\n"
        "        print(\"hello\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("whilecond"), std::string::npos);
}

TEST_F(CodeGenTest, MultipleFunctions) {
    auto ir = generateIR(
        "func add(a: Int, b: Int) -> Int { return a + b; }\n"
        "func sub(a: Int, b: Int) -> Int { return a - b; }\n"
        "func main() {\n"
        "    var x = add(10, sub(5, 3));\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("define i64 @add"), std::string::npos);
    EXPECT_NE(ir.find("define i64 @sub"), std::string::npos);
}

TEST_F(CodeGenTest, FibonacciDoneCriterion) {
    auto ir = generateIR(
        "func fibonacci(n: Int) -> Int {\n"
        "    if n <= 1 {\n"
        "        return n;\n"
        "    }\n"
        "    return fibonacci(n - 1) + fibonacci(n - 2);\n"
        "}\n"
        "func main() {\n"
        "    for i in 0..10 {\n"
        "        print(\"${fibonacci(i)}\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 5 done criterion";
    EXPECT_NE(ir.find("define i64 @fibonacci"), std::string::npos);
    EXPECT_NE(ir.find("define i32 @main"), std::string::npos);
    EXPECT_NE(ir.find("call i64 @fibonacci"), std::string::npos);
    EXPECT_NE(ir.find("chris_int_to_str"), std::string::npos);
    EXPECT_NE(ir.find("chris_print"), std::string::npos);
}

// --- Phase 6: Classes ---

TEST_F(CodeGenTest, ClassStructType) {
    auto ir = generateIR(
        "class Point {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Point {\n"
        "        return Point { x: x, y: y };\n"
        "    }\n"
        "}\n"
        "func main() { var p = Point.new(1, 2); }\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("Point"), std::string::npos);
    EXPECT_NE(ir.find("Point_new"), std::string::npos);
}

TEST_F(CodeGenTest, ClassConstructor) {
    auto ir = generateIR(
        "class Point {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Point {\n"
        "        return Point { x: x, y: y };\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var p = Point.new(3, 4);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("Point_new"), std::string::npos);
    EXPECT_NE(ir.find("chris_gc_alloc"), std::string::npos);
}

TEST_F(CodeGenTest, ClassMethodCall) {
    auto ir = generateIR(
        "class Point {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Point {\n"
        "        return Point { x: x, y: y };\n"
        "    }\n"
        "    public func sum() -> Int {\n"
        "        return this.x + this.y;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var p = Point.new(3, 4);\n"
        "    var s = p.sum();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("Point_sum"), std::string::npos);
    EXPECT_NE(ir.find("Point_new"), std::string::npos);
}

TEST_F(CodeGenTest, ClassFieldAccess) {
    auto ir = generateIR(
        "class Point {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Point {\n"
        "        return Point { x: x, y: y };\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var p = Point.new(10, 20);\n"
        "    print(\"${p.x}\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("getelementptr"), std::string::npos);
}

// --- Phase 7: Inheritance & Interfaces ---

TEST_F(CodeGenTest, InheritedFields) {
    auto ir = generateIR(
        "class Animal {\n"
        "    public var name: String;\n"
        "    public func new(name: String) -> Animal { return Animal { name: name }; }\n"
        "}\n"
        "class Dog : Animal {\n"
        "    public var breed: String;\n"
        "    public func new(name: String, breed: String) -> Dog {\n"
        "        return Dog { name: name, breed: breed };\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var d = Dog.new(\"Rex\", \"Lab\");\n"
        "    print(d.name);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("Dog_new"), std::string::npos);
    EXPECT_NE(ir.find("Animal_new"), std::string::npos);
}

TEST_F(CodeGenTest, MethodOverride) {
    auto ir = generateIR(
        "class Animal {\n"
        "    public var name: String;\n"
        "    public func new(name: String) -> Animal { return Animal { name: name }; }\n"
        "    public func toString() -> String { return this.name; }\n"
        "}\n"
        "class Dog : Animal {\n"
        "    public func new(name: String) -> Dog { return Dog { name: name }; }\n"
        "    public func toString() -> String { return this.name; }\n"
        "}\n"
        "func main() {\n"
        "    var d = Dog.new(\"Rex\");\n"
        "    print(d.toString());\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("Dog_toString"), std::string::npos);
    EXPECT_NE(ir.find("Animal_toString"), std::string::npos);
}

TEST_F(CodeGenTest, InheritanceDoneCriterion) {
    auto ir = generateIR(
        "interface Printable {\n"
        "    func toString() -> String;\n"
        "}\n"
        "class Animal : Printable {\n"
        "    public var name: String;\n"
        "    public func new(name: String) -> Animal { return Animal { name: name }; }\n"
        "    public func toString() -> String { return this.name; }\n"
        "}\n"
        "class Dog : Animal {\n"
        "    public func new(name: String) -> Dog { return Dog { name: name }; }\n"
        "    public func toString() -> String { return this.name; }\n"
        "}\n"
        "func main() {\n"
        "    var a = Animal.new(\"Cat\");\n"
        "    var d = Dog.new(\"Rex\");\n"
        "    print(a.toString());\n"
        "    print(d.toString());\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 7 done criterion";
    EXPECT_NE(ir.find("Animal_toString"), std::string::npos);
    EXPECT_NE(ir.find("Dog_toString"), std::string::npos);
    EXPECT_NE(ir.find("Animal_new"), std::string::npos);
    EXPECT_NE(ir.find("Dog_new"), std::string::npos);
}

// --- Phase 8: Null Safety ---

TEST_F(CodeGenTest, NilCoalesceCodegen) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x: String? = nil;\n"
        "    var y = x ?? \"default\";\n"
        "    print(y);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("coalesce"), std::string::npos);
}

TEST_F(CodeGenTest, ForceUnwrapCodegen) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x: String? = \"hello\";\n"
        "    var y = x!;\n"
        "    print(y);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
}

TEST_F(CodeGenTest, NullSafetyDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x: String? = nil;\n"
        "    var safe = x ?? \"default\";\n"
        "    print(safe);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 8 done criterion";
    EXPECT_NE(ir.find("coalesce"), std::string::npos);
}

// --- Phase 9: Enums & Pattern Matching ---

TEST_F(CodeGenTest, EnumVariantCodegen) {
    auto ir = generateIR(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Green;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // Color.Green should be i64 value 1
    EXPECT_NE(ir.find("store i64 1"), std::string::npos);
}

TEST_F(CodeGenTest, MatchCodegen) {
    auto ir = generateIR(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Red;\n"
        "    match c {\n"
        "        Color.Red => print(\"red\")\n"
        "        Color.Green => print(\"green\")\n"
        "        Color.Blue => print(\"blue\")\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("switch"), std::string::npos);
    EXPECT_NE(ir.find("match.Red"), std::string::npos);
    EXPECT_NE(ir.find("match.Green"), std::string::npos);
    EXPECT_NE(ir.find("match.Blue"), std::string::npos);
}

TEST_F(CodeGenTest, EnumDoneCriterion) {
    auto ir = generateIR(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Green;\n"
        "    match c {\n"
        "        Color.Red => print(\"red\")\n"
        "        Color.Green => print(\"green\")\n"
        "        Color.Blue => print(\"blue\")\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 9 done criterion";
    EXPECT_NE(ir.find("switch"), std::string::npos);
}

// --- Phase 10: Lambdas & Closures ---

TEST_F(CodeGenTest, LambdaCodegen) {
    auto ir = generateIR(
        "func main() {\n"
        "    var f = (x: Int) => x * 2;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("__lambda_"), std::string::npos);
}

TEST_F(CodeGenTest, LambdaCallCodegen) {
    auto ir = generateIR(
        "func main() {\n"
        "    var double = (x: Int) => x * 2;\n"
        "    var result = double(5);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("lambdacall"), std::string::npos);
}

TEST_F(CodeGenTest, LambdaDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var double = (x: Int) => x * 2;\n"
        "    var result = double(5);\n"
        "    print(result);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 10 done criterion";
    EXPECT_NE(ir.find("__lambda_"), std::string::npos);
    EXPECT_NE(ir.find("lambdacall"), std::string::npos);
}

// --- Phase 11: Match-as-expression & Error Handling ---

TEST_F(CodeGenTest, MatchAsExpression) {
    auto ir = generateIR(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Green;\n"
        "    var code = match c {\n"
        "        Color.Red => 0\n"
        "        Color.Green => 1\n"
        "        Color.Blue => 2\n"
        "    };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("match.result"), std::string::npos);
}

TEST_F(CodeGenTest, ThrowCodegen) {
    auto ir = generateIR(
        "func main() {\n"
        "    throw \"something went wrong\";\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_throw"), std::string::npos);
}

TEST_F(CodeGenTest, TryCatchCodegen) {
    auto ir = generateIR(
        "func main() {\n"
        "    try {\n"
        "        throw \"oops\";\n"
        "    } catch (e: Error) {\n"
        "        print(e);\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_try_begin"), std::string::npos);
    EXPECT_NE(ir.find("setjmp"), std::string::npos);
    EXPECT_NE(ir.find("try.body"), std::string::npos);
    EXPECT_NE(ir.find("catch.body"), std::string::npos);
}

TEST_F(CodeGenTest, ErrorHandlingDoneCriterion) {
    auto ir = generateIR(
        "func risky(x: Int) {\n"
        "    if x == 0 {\n"
        "        throw \"Cannot divide by zero\";\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    try {\n"
        "        risky(0);\n"
        "    } catch (e: Error) {\n"
        "        print(e);\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 11 done criterion";
    EXPECT_NE(ir.find("chris_throw"), std::string::npos);
    EXPECT_NE(ir.find("setjmp"), std::string::npos);
}

// --- Phase 12: Access Modifiers ---

TEST_F(CodeGenTest, AccessModifierDoneCriterion) {
    auto ir = generateIR(
        "class Account {\n"
        "    private var balance: Int;\n"
        "    public func new(b: Int) -> Account { return Account { balance: b }; }\n"
        "    public func getBalance() -> Int { return this.balance; }\n"
        "}\n"
        "func main() {\n"
        "    var a = Account.new(100);\n"
        "    var b = a.getBalance();\n"
        "    print(b);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 12 done criterion";
    EXPECT_NE(ir.find("Account_new"), std::string::npos);
    EXPECT_NE(ir.find("Account_getBalance"), std::string::npos);
}

// --- Phase 13: Generics ---

TEST_F(CodeGenTest, GenericsDoneCriterion) {
    auto ir = generateIR(
        "class Box<T> {\n"
        "    private var value: T;\n"
        "    public func new(value: T) -> Box {\n"
        "        return Box { value: value };\n"
        "    }\n"
        "    public func get() -> T {\n"
        "        return this.value;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var intBox: Box<Int> = Box.new(42);\n"
        "    var strBox: Box<String> = Box.new(\"hello\");\n"
        "    print(intBox.get());\n"
        "    print(strBox.get());\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 13 done criterion";
    // Should have monomorphized Box<Int> and Box<String>
    EXPECT_NE(ir.find("Box<Int>"), std::string::npos);
    EXPECT_NE(ir.find("Box<String>"), std::string::npos);
    EXPECT_NE(ir.find("Box<Int>_new"), std::string::npos);
    EXPECT_NE(ir.find("Box<Int>_get"), std::string::npos);
    EXPECT_NE(ir.find("Box<String>_new"), std::string::npos);
    EXPECT_NE(ir.find("Box<String>_get"), std::string::npos);
}

// --- Phase 14: Operator Overloading ---

TEST_F(CodeGenTest, OperatorOverloadDoneCriterion) {
    auto ir = generateIR(
        "class Vec {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Vec {\n"
        "        return Vec { x: x, y: y };\n"
        "    }\n"
        "    operator +(other: Vec) -> Vec {\n"
        "        return Vec.new(this.x + other.x, this.y + other.y);\n"
        "    }\n"
        "    operator ==(other: Vec) -> Bool {\n"
        "        return this.x == other.x && this.y == other.y;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var a: Vec = Vec.new(1, 2);\n"
        "    var b: Vec = Vec.new(3, 4);\n"
        "    var c: Vec = a + b;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 14 done criterion";
    EXPECT_NE(ir.find("Vec_operator+"), std::string::npos);
    EXPECT_NE(ir.find("Vec_operator=="), std::string::npos);
}

// --- Phase 15: Arrays ---

TEST_F(CodeGenTest, ArrayDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var numbers = [10, 20, 30, 40, 50];\n"
        "    var first: Int = numbers[0];\n"
        "    var len: Int = numbers.length;\n"
        "    print(first);\n"
        "    print(len);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 15 done criterion";
    EXPECT_NE(ir.find("chris_array_alloc"), std::string::npos);
    EXPECT_NE(ir.find("chris_array_bounds_check"), std::string::npos);
}

// --- Phase 16: For-in over arrays ---

TEST_F(CodeGenTest, ForInArrayDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var numbers = [10, 20, 30, 40, 50];\n"
        "    for n in numbers {\n"
        "        print(n);\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 16 done criterion";
    EXPECT_NE(ir.find("chris_array_alloc"), std::string::npos);
    EXPECT_NE(ir.find("forcond"), std::string::npos);
    EXPECT_NE(ir.find("forbody"), std::string::npos);
}

// --- Phase 17: C FFI (extern functions) ---

TEST_F(CodeGenTest, ExternFuncDoneCriterion) {
    auto ir = generateIR(
        "extern func puts(s: String) -> Int;\n"
        "func main() {\n"
        "    puts(\"Hello from extern\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 17 done criterion";
    EXPECT_NE(ir.find("declare"), std::string::npos);
    EXPECT_NE(ir.find("puts"), std::string::npos);
}

// --- Phase 18: Array Index Assignment ---

TEST_F(CodeGenTest, ArrayIndexAssignDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    arr[1] = 99;\n"
        "    print(arr[1]);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 18 done criterion";
    EXPECT_NE(ir.find("chris_array_bounds_check"), std::string::npos);
}

// --- Phase 19: Multi-file Imports ---
// Note: full import resolution is tested end-to-end via the compiler driver.
// Here we test that import decls in code don't break codegen.

TEST_F(CodeGenTest, ImportDeclCodegen) {
    auto ir = generateIR(
        "import \"mathlib.chr\";\n"
        "func main() {\n"
        "    print(42);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 19 import test";
    EXPECT_NE(ir.find("main"), std::string::npos);
}

// --- Phase 20: Type Conversion Methods ---

TEST_F(CodeGenTest, IntToString) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var s = x.toString();\n"
        "    print(s);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Int.toString()";
    EXPECT_NE(ir.find("chris_int_to_str"), std::string::npos);
}

TEST_F(CodeGenTest, IntToFloat) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var f = x.toFloat();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Int.toFloat()";
    EXPECT_NE(ir.find("sitofp"), std::string::npos);
}

TEST_F(CodeGenTest, FloatToInt) {
    auto ir = generateIR(
        "func main() {\n"
        "    var f: Float = 3.14;\n"
        "    var x = f.toInt();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Float.toInt()";
    EXPECT_NE(ir.find("fptosi"), std::string::npos);
}

TEST_F(CodeGenTest, StringToInt) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s: String = \"42\";\n"
        "    var x = s.toInt();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for String.toInt()";
    EXPECT_NE(ir.find("chris_str_to_int"), std::string::npos);
}

TEST_F(CodeGenTest, TypeConversionDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var s = x.toString();\n"
        "    var f = x.toFloat();\n"
        "    print(s);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 20 done criterion";
    EXPECT_NE(ir.find("chris_int_to_str"), std::string::npos);
    EXPECT_NE(ir.find("sitofp"), std::string::npos);
}

// --- Phase 21: String Methods ---

TEST_F(CodeGenTest, StringContains) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s: String = \"hello world\";\n"
        "    var b = s.contains(\"world\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for String.contains()";
    EXPECT_NE(ir.find("chris_str_contains"), std::string::npos);
}

TEST_F(CodeGenTest, StringTrim) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s: String = \"  hello  \";\n"
        "    var t = s.trim();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for String.trim()";
    EXPECT_NE(ir.find("chris_str_trim"), std::string::npos);
}

TEST_F(CodeGenTest, StringSubstring) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s: String = \"hello\";\n"
        "    var sub = s.substring(0, 3);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for String.substring()";
    EXPECT_NE(ir.find("chris_str_substring"), std::string::npos);
}

TEST_F(CodeGenTest, StringReplace) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s: String = \"hello world\";\n"
        "    var r = s.replace(\"world\", \"there\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for String.replace()";
    EXPECT_NE(ir.find("chris_str_replace"), std::string::npos);
}

TEST_F(CodeGenTest, StringMethodsDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s: String = \"Hello World\";\n"
        "    var b = s.contains(\"World\");\n"
        "    var t = s.trim();\n"
        "    var u = s.toUpper();\n"
        "    var l = s.toLower();\n"
        "    var i = s.indexOf(\"World\");\n"
        "    var c = s.charAt(0);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 21 done criterion";
    EXPECT_NE(ir.find("chris_str_contains"), std::string::npos);
    EXPECT_NE(ir.find("chris_str_to_upper"), std::string::npos);
    EXPECT_NE(ir.find("chris_str_to_lower"), std::string::npos);
}

// --- Phase 22: Array Methods ---

TEST_F(CodeGenTest, ArrayPush) {
    auto ir = generateIR(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    arr.push(4);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Array.push()";
    EXPECT_NE(ir.find("chris_array_push"), std::string::npos);
}

TEST_F(CodeGenTest, ArrayPop) {
    auto ir = generateIR(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    var x = arr.pop();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Array.pop()";
    EXPECT_NE(ir.find("chris_array_pop"), std::string::npos);
}

TEST_F(CodeGenTest, ArrayReverse) {
    auto ir = generateIR(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    arr.reverse();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Array.reverse()";
    EXPECT_NE(ir.find("chris_array_reverse"), std::string::npos);
}

TEST_F(CodeGenTest, ArrayMethodsDoneCriterion) {
    auto ir = generateIR(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    arr.push(40);\n"
        "    var x = arr.pop();\n"
        "    arr.reverse();\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 22 done criterion";
    EXPECT_NE(ir.find("chris_array_push"), std::string::npos);
    EXPECT_NE(ir.find("chris_array_pop"), std::string::npos);
    EXPECT_NE(ir.find("chris_array_reverse"), std::string::npos);
}

// --- Phase 23: Interfaces ---

TEST_F(CodeGenTest, InterfaceConformance) {
    auto ir = generateIR(
        "interface Printable {\n"
        "    func display() -> String;\n"
        "}\n"
        "class User : Printable {\n"
        "    public var name: String;\n"
        "    public func new(name: String) -> User {\n"
        "        return User { name: name };\n"
        "    }\n"
        "    public func display() -> String {\n"
        "        return this.name;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var u = User.new(\"Chris\");\n"
        "    print(u.display());\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for interface conformance";
    EXPECT_NE(ir.find("User_display"), std::string::npos);
}

// --- Phase 24: let immutability ---

TEST_F(CodeGenTest, LetDeclaration) {
    auto ir = generateIR(
        "func main() {\n"
        "    let x: Int = 42;\n"
        "    print(x);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for let declaration";
    EXPECT_NE(ir.find("main"), std::string::npos);
}

TEST_F(CodeGenTest, InterfaceDoneCriterion) {
    auto ir = generateIR(
        "interface Serializable {\n"
        "    func serialize() -> String;\n"
        "}\n"
        "class Config : Serializable {\n"
        "    public var host: String;\n"
        "    public func new(host: String) -> Config {\n"
        "        return Config { host: host };\n"
        "    }\n"
        "    public func serialize() -> String {\n"
        "        return this.host;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var c = Config.new(\"localhost\");\n"
        "    print(c.serialize());\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 23 done criterion";
    EXPECT_NE(ir.find("Config_serialize"), std::string::npos);
}

// --- Phase 25: Enums with associated values ---

TEST_F(CodeGenTest, EnumAssociatedValueConstruction) {
    auto ir = generateIR(
        "enum Result {\n"
        "    Ok(Int),\n"
        "    Error(String)\n"
        "}\n"
        "func main() {\n"
        "    var r = Result.Ok(42);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for enum with associated values";
    EXPECT_NE(ir.find("Result_Enum"), std::string::npos);
}

TEST_F(CodeGenTest, EnumMatchDestructuring) {
    auto ir = generateIR(
        "enum Result {\n"
        "    Ok(Int),\n"
        "    Error(String)\n"
        "}\n"
        "func main() {\n"
        "    var r = Result.Ok(42);\n"
        "    match r {\n"
        "        Ok(val) => { print(val); }\n"
        "        Error(msg) => { print(msg); }\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for enum match destructuring";
    EXPECT_NE(ir.find("match.Ok"), std::string::npos);
    EXPECT_NE(ir.find("match.Error"), std::string::npos);
}

TEST_F(CodeGenTest, ADTDoneCriterion) {
    auto ir = generateIR(
        "enum Result {\n"
        "    Ok(Int),\n"
        "    Error(String)\n"
        "}\n"
        "func main() {\n"
        "    var ok = Result.Ok(42);\n"
        "    var err = Result.Error(\"fail\");\n"
        "    match ok {\n"
        "        Ok(v) => { print(v); }\n"
        "        Error(m) => { print(m); }\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for Phase 25 done criterion";
    EXPECT_NE(ir.find("Result_Enum"), std::string::npos);
    EXPECT_NE(ir.find("match.Ok"), std::string::npos);
}

// --- Phase 26: Match as expression ---

// --- Phase 27: Sized integer types ---

TEST_F(CodeGenTest, SizedIntegerTypes) {
    auto ir = generateIR(
        "func main() {\n"
        "    var a: Int8 = 42;\n"
        "    var b: Int16 = 1000;\n"
        "    var c: Int32 = 100000;\n"
        "    var d: UInt = 42;\n"
        "    var e: Float32 = 3.14;\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for sized integer types";
    EXPECT_NE(ir.find("i8"), std::string::npos);
    EXPECT_NE(ir.find("i16"), std::string::npos);
    EXPECT_NE(ir.find("i32"), std::string::npos);
}

// --- Phase 28: Char type ---

TEST_F(CodeGenTest, CharLiteral) {
    auto ir = generateIR(
        "func main() {\n"
        "    var c: Char = 'A';\n"
        "    print(c);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for char literal";
    EXPECT_NE(ir.find("chris_char_to_str"), std::string::npos);
}

// --- Phase 34: Function params with sized types ---

TEST_F(CodeGenTest, FunctionWithSizedParams) {
    auto ir = generateIR(
        "func add8(a: Int8, b: Int8) -> Int8 {\n"
        "    return a + b;\n"
        "}\n"
        "func main() {\n"
        "    var result = add8(10, 20);\n"
        "    print(result);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for sized params";
    EXPECT_NE(ir.find("add8"), std::string::npos);
}

TEST_F(CodeGenTest, ReturnCoercionSizedTypes) {
    auto ir = generateIR(
        "func getInt8() -> Int8 {\n"
        "    return 42;\n"
        "}\n"
        "func main() {\n"
        "    var n = getInt8();\n"
        "    print(n);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for return coercion";
    EXPECT_NE(ir.find("getInt8"), std::string::npos);
}

TEST_F(CodeGenTest, HigherOrderFunctionTypeParam) {
    auto ir = generateIR(
        "func apply(x: Int, f: (Int) -> Int) -> Int {\n"
        "    return f(x);\n"
        "}\n"
        "func main() {\n"
        "    var result = apply(5, (x) => x * x);\n"
        "    print(result);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for function type param";
    EXPECT_NE(ir.find("apply"), std::string::npos);
}

TEST_F(CodeGenTest, IfExpression) {
    auto ir = generateIR(
        "func main() {\n"
        "    var x = 10;\n"
        "    var y = if x > 5 { 1 } else { 0 };\n"
        "    print(y);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for if expression";
    EXPECT_NE(ir.find("ifexpr"), std::string::npos);
}

TEST_F(CodeGenTest, ArrayReturnByValue) {
    auto ir = generateIR(
        "func makeArr() -> Array<Int> {\n"
        "    var result = [10, 20, 30];\n"
        "    return result;\n"
        "}\n"
        "func main() {\n"
        "    var nums = makeArr();\n"
        "    print(nums[0]);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for array return by value";
}

TEST_F(CodeGenTest, ArrayParamAndReturn) {
    auto ir = generateIR(
        "func sum(nums: Array<Int>) -> Int {\n"
        "    var total = 0;\n"
        "    for i in 0..nums.length {\n"
        "        total += nums[i];\n"
        "    }\n"
        "    return total;\n"
        "}\n"
        "func main() {\n"
        "    var data = [1, 2, 3];\n"
        "    print(sum(data));\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for array param";
}

TEST_F(CodeGenTest, ClassConstructorAllocation) {
    auto ir = generateIR(
        "class Counter {\n"
        "    public var count: Int;\n"
        "    public func new() -> Counter {\n"
        "        this.count = 0;\n"
        "    }\n"
        "    public func getCount() -> Int {\n"
        "        return this.count;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var c = Counter.new();\n"
        "    print(c.getCount());\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for class constructor";
    EXPECT_NE(ir.find("chris_gc_alloc"), std::string::npos);
}

TEST_F(CodeGenTest, ForEachStringArray) {
    auto ir = generateIR(
        "func main() {\n"
        "    var words = [\"hello\", \"world\"];\n"
        "    words.forEach((w) => print(w));\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for forEach on string array";
}

TEST_F(CodeGenTest, ClosureReturnLambdaInference) {
    // Type checker test — should not produce errors
    auto ir = generateIR(
        "func apply(x: Int, f: (Int) -> Int) -> Int {\n"
        "    return f(x);\n"
        "}\n"
        "func main() {\n"
        "    var r = apply(3, (x) => x + 1);\n"
        "    print(r);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for closure return lambda inference";
}

TEST_F(CodeGenTest, StringSplitElementType) {
    auto ir = generateIR(
        "func main() {\n"
        "    var s = \"hello world\";\n"
        "    var parts = s.split(\" \");\n"
        "    print(parts[0]);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for string split";
}

TEST_F(CodeGenTest, LambdaFilterReturnType) {
    auto ir = generateIR(
        "func main() {\n"
        "    var nums = [1, 2, 3, 4, 5, 6];\n"
        "    var evens = nums.filter((x) => x % 2 == 0);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for filter lambda";
}

TEST_F(CodeGenTest, AssignmentCoercionSizedTypes) {
    auto ir = generateIR(
        "func main() {\n"
        "    var a: Int8 = 10;\n"
        "    a = a + 5;\n"
        "    print(a);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for assignment coercion";
}

// --- Phase 33: Finally block fix ---

TEST_F(CodeGenTest, TryCatchFinally) {
    auto ir = generateIR(
        "func main() {\n"
        "    try {\n"
        "        print(\"try\");\n"
        "        throw \"error\";\n"
        "    } catch (e: String) {\n"
        "        print(e);\n"
        "    } finally {\n"
        "        print(\"finally\");\n"
        "    }\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for try/catch/finally";
    EXPECT_NE(ir.find("finally"), std::string::npos);
}

// --- Phase 31: String interpolation with sized/char types ---

TEST_F(CodeGenTest, StringInterpolationSizedTypes) {
    auto ir = generateIR(
        "func main() {\n"
        "    var a: Int8 = 42;\n"
        "    var c: Char = 'A';\n"
        "    var f: Float32 = 3.14;\n"
        "    print(\"a=${a}, c=${c}, f=${f}\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for string interp with sized types";
    EXPECT_NE(ir.find("chris_char_to_str"), std::string::npos);
    EXPECT_NE(ir.find("chris_int_to_str"), std::string::npos);
    EXPECT_NE(ir.find("chris_float_to_str"), std::string::npos);
}

TEST_F(CodeGenTest, MatchExpressionWithInts) {
    auto ir = generateIR(
        "enum Color {\n"
        "    Red,\n"
        "    Green,\n"
        "    Blue\n"
        "}\n"
        "func main() {\n"
        "    var c = Color.Green;\n"
        "    var idx = match c {\n"
        "        Red => 0\n"
        "        Green => 1\n"
        "        Blue => 2\n"
        "    };\n"
        "    print(idx);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors()) << "Codegen failed for match as expression";
    EXPECT_NE(ir.find("match.result"), std::string::npos);
}
