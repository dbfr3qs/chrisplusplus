#include <gtest/gtest.h>
#include "sema/type_checker.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "common/diagnostic.h"

using namespace chris;

class TypeCheckerTest : public ::testing::Test {
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

    bool hasError(const std::string& code) {
        return diag.hasErrors() || findError(code);
    }

    bool findError(const std::string& code) {
        for (const auto& d : diag.diagnostics()) {
            if (d.code == code) return true;
        }
        return false;
    }
};

// --- Valid Programs ---

TEST_F(TypeCheckerTest, SimpleVarDecl) {
    parseAndCheck("var x = 42;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, VarDeclWithType) {
    parseAndCheck("var x: Int = 42;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LetDecl) {
    parseAndCheck("let x = 42;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringVar) {
    parseAndCheck("var name = \"Chris\";");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, BoolVar) {
    parseAndCheck("var flag = true;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, FloatVar) {
    parseAndCheck("var pi: Float = 3.14;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, NullableVar) {
    parseAndCheck("var x: Int? = nil;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, NullableWithValue) {
    parseAndCheck("var x: Int? = 42;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ArithmeticExpressions) {
    parseAndCheck("var x = 1 + 2 * 3;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, FloatArithmetic) {
    parseAndCheck("var x = 1.5 + 2.5;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, MixedNumericArithmetic) {
    parseAndCheck("var x: Float = 1 + 2.5;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ComparisonReturnsBool) {
    parseAndCheck("var x = 1 > 2;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LogicalExpressions) {
    parseAndCheck("var x = true && false;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, UnaryNegation) {
    parseAndCheck("var x = -42;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, UnaryNot) {
    parseAndCheck("var x = !true;");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringConcatenation) {
    parseAndCheck("var x = \"hello\" + \" world\";");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, PrintString) {
    parseAndCheck("print(\"hello\");");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, PrintStringInterpolation) {
    parseAndCheck(
        "var name = \"Chris\";\n"
        "print(\"Hello, ${name}!\");\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, FunctionDecl) {
    parseAndCheck("func add(a: Int, b: Int) -> Int { return a + b; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, FunctionCall) {
    parseAndCheck(
        "func add(a: Int, b: Int) -> Int { return a + b; }\n"
        "func main() { var x = add(1, 2); }\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, IfStatement) {
    parseAndCheck("func main() { var x = 10; if x > 5 { print(\"big\"); } }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, WhileLoop) {
    parseAndCheck("func main() { var x = 10; while x > 0 { x = x - 1; } }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, VoidReturn) {
    parseAndCheck("func foo() { return; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, MutableAssignment) {
    parseAndCheck("var x = 10; x = 20;");
    EXPECT_FALSE(diag.hasErrors());
}

// --- Type Errors ---

TEST_F(TypeCheckerTest, TypeMismatchVarDecl) {
    parseAndCheck("var x: Int = \"hello\";");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3003"));
}

TEST_F(TypeCheckerTest, TypeMismatchAssignment) {
    parseAndCheck("var x = 42; x = \"hello\";");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3003"));
}

TEST_F(TypeCheckerTest, NilToNonNullable) {
    parseAndCheck("var x: Int = nil;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3003"));
}

TEST_F(TypeCheckerTest, InferFromNil) {
    parseAndCheck("var x = nil;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3004"));
}

TEST_F(TypeCheckerTest, UndefinedVariable) {
    parseAndCheck("var x = y;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3009"));
}

TEST_F(TypeCheckerTest, ArithmeticOnStrings) {
    parseAndCheck("var x = \"hello\" - \"world\";");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3010"));
}

TEST_F(TypeCheckerTest, ArithmeticOnBool) {
    parseAndCheck("var x = true + false;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3010"));
}

TEST_F(TypeCheckerTest, ComparisonOnStrings) {
    parseAndCheck("var x = \"a\" > \"b\";");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3010"));
}

TEST_F(TypeCheckerTest, LogicalOnInts) {
    parseAndCheck("var x = 1 && 2;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3010"));
}

TEST_F(TypeCheckerTest, UnaryNegOnString) {
    parseAndCheck("var x = -\"hello\";");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3011"));
}

TEST_F(TypeCheckerTest, UnaryNotOnInt) {
    parseAndCheck("var x = !42;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3011"));
}

TEST_F(TypeCheckerTest, IfConditionNotBool) {
    parseAndCheck("func main() { if 42 { } }");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3007"));
}

TEST_F(TypeCheckerTest, WhileConditionNotBool) {
    parseAndCheck("func main() { while 42 { } }");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3007"));
}

TEST_F(TypeCheckerTest, WrongReturnType) {
    parseAndCheck("func foo() -> Int { return \"hello\"; }");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3008"));
}

TEST_F(TypeCheckerTest, MissingReturnValue) {
    parseAndCheck("func foo() -> Int { return; }");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3008"));
}

TEST_F(TypeCheckerTest, WrongArgCount) {
    parseAndCheck(
        "func add(a: Int, b: Int) -> Int { return a + b; }\n"
        "func main() { add(1); }\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3013"));
}

TEST_F(TypeCheckerTest, WrongArgType) {
    parseAndCheck(
        "func add(a: Int, b: Int) -> Int { return a + b; }\n"
        "func main() { add(1, \"hello\"); }\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3014"));
}

TEST_F(TypeCheckerTest, PrintAcceptsAnyType) {
    parseAndCheck("print(42);");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ImmutableAssignment) {
    parseAndCheck("let x = 42; x = 10;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3015"));
}

TEST_F(TypeCheckerTest, UnknownType) {
    parseAndCheck("var x: Foo = 42;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3016"));
}

TEST_F(TypeCheckerTest, DuplicateVariable) {
    parseAndCheck("var x = 1; var x = 2;");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3006"));
}

TEST_F(TypeCheckerTest, DuplicateFunction) {
    parseAndCheck("func foo() { } func foo() { }");
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3001"));
}

// --- Scoping ---

TEST_F(TypeCheckerTest, BlockScoping) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 10;\n"
        "    if true {\n"
        "        var y = 20;\n"
        "        var z = x + y;\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ShadowingInInnerScope) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 10;\n"
        "    if true {\n"
        "        var x = \"hello\";\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 5: Range & For Loop Type Checking ---

TEST_F(TypeCheckerTest, ForInRange) {
    parseAndCheck(
        "func main() {\n"
        "    for i in 0..10 {\n"
        "        var x = i + 1;\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, RangeNonIntStart) {
    parseAndCheck(
        "func main() { for i in \"a\"..10 { } }\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3017"));
}

TEST_F(TypeCheckerTest, RecursiveFunction) {
    parseAndCheck(
        "func fib(n: Int) -> Int {\n"
        "    if n <= 1 { return n; }\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, FibonacciDoneCriterion) {
    parseAndCheck(
        "func fibonacci(n: Int) -> Int {\n"
        "    if n <= 1 { return n; }\n"
        "    return fibonacci(n - 1) + fibonacci(n - 2);\n"
        "}\n"
        "func main() {\n"
        "    for i in 0..10 {\n"
        "        print(\"${fibonacci(i)}\");\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors()) << "Type checker rejected Phase 5 done criterion";
}

// --- Phase 6: Classes ---

TEST_F(TypeCheckerTest, ClassDeclaration) {
    parseAndCheck(
        "class Point {\n"
        "    var x: Int;\n"
        "    var y: Int;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ClassConstructorReturnType) {
    parseAndCheck(
        "class Point {\n"
        "    var x: Int;\n"
        "    func new(x: Int) -> Point {\n"
        "        return Point { x: x };\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ClassThisInMethod) {
    parseAndCheck(
        "class Point {\n"
        "    var x: Int;\n"
        "    func getX() -> Int { return this.x; }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ClassUnknownField) {
    parseAndCheck(
        "class Point { var x: Int; }\n"
        "func main() {\n"
        "    var p = Point { x: 1 };\n"
        "    var z = p.nonexistent;\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3018"));
}

// --- Phase 7: Inheritance & Interfaces ---

TEST_F(TypeCheckerTest, InterfaceDeclaration) {
    parseAndCheck(
        "interface Printable {\n"
        "    func toString() -> String;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ClassInheritance) {
    parseAndCheck(
        "class Animal {\n"
        "    var name: String;\n"
        "}\n"
        "class Dog : Animal {\n"
        "    var breed: String;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, InheritedFieldAccess) {
    parseAndCheck(
        "class Animal {\n"
        "    var name: String;\n"
        "    func new(name: String) -> Animal { return Animal { name: name }; }\n"
        "}\n"
        "class Dog : Animal {\n"
        "    func new(name: String) -> Dog { return Dog { name: name }; }\n"
        "    func getName() -> String { return this.name; }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, InheritanceDoneCriterion) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 8: Null Safety ---

TEST_F(TypeCheckerTest, NilCoalesceType) {
    parseAndCheck(
        "func main() {\n"
        "    var x: String? = nil;\n"
        "    var y = x ?? \"default\";\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ForceUnwrapType) {
    parseAndCheck(
        "func main() {\n"
        "    var x: String? = \"hello\";\n"
        "    var y = x!;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, OptionalChainType) {
    parseAndCheck(
        "class Point { var x: Int; var y: Int; }\n"
        "func main() {\n"
        "    var p: Point? = nil;\n"
        "    var x = p?.x;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 9: Enums & Pattern Matching ---

TEST_F(TypeCheckerTest, EnumDeclaration) {
    parseAndCheck("enum Color { Red, Green, Blue }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, EnumVariantAccess) {
    parseAndCheck(
        "enum Color { Red, Green, Blue }\n"
        "func main() { var c = Color.Red; }\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, MatchExpression) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, EnumDoneCriterion) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 10: Lambdas & Closures ---

TEST_F(TypeCheckerTest, LambdaExpression) {
    parseAndCheck("func main() { var f = (x: Int) => x * 2; }");
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LambdaCall) {
    parseAndCheck(
        "func main() {\n"
        "    var f = (x: Int) => x * 2;\n"
        "    var result = f(5);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LambdaMultipleParams) {
    parseAndCheck(
        "func main() {\n"
        "    var add = (a: Int, b: Int) => a + b;\n"
        "    var sum = add(3, 4);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LambdaDoneCriterion) {
    parseAndCheck(
        "func main() {\n"
        "    var double = (x: Int) => x * 2;\n"
        "    var result = double(5);\n"
        "    print(result);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, NullSafetyDoneCriterion) {
    parseAndCheck(
        "func main() {\n"
        "    var x: String? = nil;\n"
        "    var safe = x ?? \"default\";\n"
        "    print(safe);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 11: Match-as-expression & Error Handling ---

TEST_F(TypeCheckerTest, MatchAsExpression) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ThrowStatement) {
    parseAndCheck(
        "func main() {\n"
        "    throw \"something went wrong\";\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, TryCatchStatement) {
    parseAndCheck(
        "func main() {\n"
        "    try {\n"
        "        throw \"oops\";\n"
        "    } catch (e: Error) {\n"
        "        print(e);\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, TryCatchFinally) {
    parseAndCheck(
        "func main() {\n"
        "    try {\n"
        "        throw \"oops\";\n"
        "    } catch (e: Error) {\n"
        "        print(e);\n"
        "    } finally {\n"
        "        print(\"done\");\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ErrorHandlingDoneCriterion) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 12: Access Modifiers ---

TEST_F(TypeCheckerTest, PrivateFieldAccessDenied) {
    parseAndCheck(
        "class Secret {\n"
        "    private var value: Int;\n"
        "    public func new(v: Int) -> Secret { return Secret { value: v }; }\n"
        "}\n"
        "func main() {\n"
        "    var s = Secret.new(42);\n"
        "    var v = s.value;\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3023"));
}

TEST_F(TypeCheckerTest, PublicFieldAccessAllowed) {
    parseAndCheck(
        "class Open {\n"
        "    public var value: Int;\n"
        "    public func new(v: Int) -> Open { return Open { value: v }; }\n"
        "}\n"
        "func main() {\n"
        "    var o = Open.new(42);\n"
        "    var v = o.value;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, PrivateMethodAccessDenied) {
    parseAndCheck(
        "class Secret {\n"
        "    private var x: Int;\n"
        "    private func hidden() -> Int { return 42; }\n"
        "    public func new(x: Int) -> Secret { return Secret { x: x }; }\n"
        "}\n"
        "func main() {\n"
        "    var s = Secret.new(1);\n"
        "    var v = s.hidden();\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3023"));
}

TEST_F(TypeCheckerTest, DefaultAccessIsPrivate) {
    parseAndCheck(
        "class Secret {\n"
        "    var value: Int;\n"
        "    public func new(v: Int) -> Secret { return Secret { value: v }; }\n"
        "}\n"
        "func main() {\n"
        "    var s = Secret.new(42);\n"
        "    var v = s.value;\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3023"));
}

TEST_F(TypeCheckerTest, ProtectedAccessInSubclass) {
    parseAndCheck(
        "class Base {\n"
        "    protected var secret: Int;\n"
        "    public func new(v: Int) -> Base { return Base { secret: v }; }\n"
        "}\n"
        "class Child : Base {\n"
        "    public func new(v: Int) -> Child { return Child { secret: v }; }\n"
        "    public func getSecret() -> Int { return this.secret; }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ProtectedAccessDeniedExternally) {
    parseAndCheck(
        "class Base {\n"
        "    protected var secret: Int;\n"
        "    public func new(v: Int) -> Base { return Base { secret: v }; }\n"
        "}\n"
        "func main() {\n"
        "    var b = Base.new(42);\n"
        "    var v = b.secret;\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3023"));
}

TEST_F(TypeCheckerTest, AccessModifierDoneCriterion) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 13: Generics ---

TEST_F(TypeCheckerTest, GenericClassDeclaration) {
    parseAndCheck(
        "class Box<T> {\n"
        "    public var value: T;\n"
        "    public func new(value: T) -> Box {\n"
        "        return Box { value: value };\n"
        "    }\n"
        "    public func get() -> T {\n"
        "        return this.value;\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, GenericTypeAnnotation) {
    parseAndCheck(
        "class Box<T> {\n"
        "    public var value: T;\n"
        "    public func new(value: T) -> Box {\n"
        "        return Box { value: value };\n"
        "    }\n"
        "    public func get() -> T {\n"
        "        return this.value;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var b: Box<Int> = Box.new(42);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, GenericWrongTypeArgCount) {
    parseAndCheck(
        "class Box<T> {\n"
        "    public var value: T;\n"
        "}\n"
        "func main() {\n"
        "    var b: Box<Int, String> = Box { value: 42 };\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
    EXPECT_TRUE(findError("E3024"));
}

TEST_F(TypeCheckerTest, GenericsDoneCriterion) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 14: Operator Overloading ---

TEST_F(TypeCheckerTest, OperatorOverloadTypeCheck) {
    parseAndCheck(
        "class Vec {\n"
        "    public var x: Int;\n"
        "    public var y: Int;\n"
        "    public func new(x: Int, y: Int) -> Vec {\n"
        "        return Vec { x: x, y: y };\n"
        "    }\n"
        "    operator +(other: Vec) -> Vec {\n"
        "        return Vec.new(this.x + other.x, this.y + other.y);\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var a: Vec = Vec.new(1, 2);\n"
        "    var b: Vec = Vec.new(3, 4);\n"
        "    var c: Vec = a + b;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, OperatorOverloadEquality) {
    parseAndCheck(
        "class Vec {\n"
        "    public var x: Int;\n"
        "    operator ==(other: Vec) -> Bool {\n"
        "        return this.x == other.x;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var a: Vec = Vec { x: 1 };\n"
        "    var b: Vec = Vec { x: 1 };\n"
        "    var eq: Bool = a == b;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, OperatorOverloadDoneCriterion) {
    parseAndCheck(
        "class Vector2 {\n"
        "    public var x: Float;\n"
        "    public var y: Float;\n"
        "    public func new(x: Float, y: Float) -> Vector2 {\n"
        "        return Vector2 { x: x, y: y };\n"
        "    }\n"
        "    operator +(other: Vector2) -> Vector2 {\n"
        "        return Vector2.new(this.x + other.x, this.y + other.y);\n"
        "    }\n"
        "    operator ==(other: Vector2) -> Bool {\n"
        "        return this.x == other.x && this.y == other.y;\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var a: Vector2 = Vector2.new(1.0, 2.0);\n"
        "    var b: Vector2 = Vector2.new(3.0, 4.0);\n"
        "    var c: Vector2 = a + b;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 15: Arrays ---

TEST_F(TypeCheckerTest, ArrayLiteralTypeCheck) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ArrayIndexTypeCheck) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    var x: Int = arr[0];\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ArrayLengthTypeCheck) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    var len: Int = arr.length;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ArrayDoneCriterion) {
    parseAndCheck(
        "func main() {\n"
        "    var numbers = [10, 20, 30, 40, 50];\n"
        "    var first: Int = numbers[0];\n"
        "    var len: Int = numbers.length;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 16: For-in over arrays ---

TEST_F(TypeCheckerTest, ForInArrayTypeCheck) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    for x in arr {\n"
        "        print(x);\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ForInArrayDoneCriterion) {
    parseAndCheck(
        "func main() {\n"
        "    var numbers = [10, 20, 30, 40, 50];\n"
        "    var sum: Int = 0;\n"
        "    for n in numbers {\n"
        "        sum = sum + n;\n"
        "    }\n"
        "    print(sum);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 17: C FFI (extern functions) ---

TEST_F(TypeCheckerTest, ExternFuncTypeCheck) {
    parseAndCheck(
        "extern func puts(s: String) -> Int;\n"
        "func main() {\n"
        "    puts(\"hello\");\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ExternFuncDoneCriterion) {
    parseAndCheck(
        "extern func puts(s: String) -> Int;\n"
        "extern func abs(x: Int) -> Int;\n"
        "func main() {\n"
        "    puts(\"Hello from extern\");\n"
        "    var x: Int = abs(0 - 42);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 18: Array Index Assignment ---

TEST_F(TypeCheckerTest, ArrayIndexAssignment) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    arr[0] = 99;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ArrayIndexAssignDoneCriterion) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [10, 20, 30];\n"
        "    arr[1] = 99;\n"
        "    var x: Int = arr[1];\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 19: Multi-file Imports ---
// Note: import resolution is tested end-to-end via the compiler driver.
// Here we test that import declarations parse and type-check without error.

TEST_F(TypeCheckerTest, ImportDeclTypeCheck) {
    parseAndCheck(
        "import \"mathlib.chr\";\n"
        "func main() {\n"
        "    print(1);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 20: Type Conversion Methods ---

TEST_F(TypeCheckerTest, IntToString) {
    parseAndCheck(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var s: String = x.toString();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, IntToFloat) {
    parseAndCheck(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var f: Float = x.toFloat();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, FloatToInt) {
    parseAndCheck(
        "func main() {\n"
        "    var f: Float = 3.14;\n"
        "    var x: Int = f.toInt();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringToInt) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"42\";\n"
        "    var x: Int = s.toInt();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringLength) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"hello\";\n"
        "    var n: Int = s.length;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, TypeConversionDoneCriterion) {
    parseAndCheck(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    var s: String = x.toString();\n"
        "    var f: Float = x.toFloat();\n"
        "    var back: Int = f.toInt();\n"
        "    var str: String = \"123\";\n"
        "    var parsed: Int = str.toInt();\n"
        "    var len: Int = str.length;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 21: String Methods ---

TEST_F(TypeCheckerTest, StringContains) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"hello world\";\n"
        "    var b: Bool = s.contains(\"world\");\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringStartsEndsWith) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"hello\";\n"
        "    var a: Bool = s.startsWith(\"he\");\n"
        "    var b: Bool = s.endsWith(\"lo\");\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringSubstringReplace) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"hello world\";\n"
        "    var sub: String = s.substring(0, 5);\n"
        "    var rep: String = s.replace(\"world\", \"there\");\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringTrimUpperLower) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"  hello  \";\n"
        "    var t: String = s.trim();\n"
        "    var u: String = s.toUpper();\n"
        "    var l: String = s.toLower();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringIndexOfCharAt) {
    parseAndCheck(
        "func main() {\n"
        "    var s: String = \"hello\";\n"
        "    var i: Int = s.indexOf(\"ll\");\n"
        "    var c: String = s.charAt(0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 22: Array Methods ---

TEST_F(TypeCheckerTest, ArrayPushPop) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    arr.push(4);\n"
        "    var x = arr.pop();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ArrayReverse) {
    parseAndCheck(
        "func main() {\n"
        "    var arr = [1, 2, 3];\n"
        "    arr.reverse();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 23: Interfaces ---

TEST_F(TypeCheckerTest, InterfaceConformanceValid) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, InterfaceMissingMethod) {
    parseAndCheck(
        "interface Printable {\n"
        "    func display() -> String;\n"
        "}\n"
        "class User : Printable {\n"
        "    public var name: String;\n"
        "    func new(name: String) -> User {\n"
        "        return User { name: name };\n"
        "    }\n"
        "}\n"
        "func main() {}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, InterfaceMultipleMethods) {
    parseAndCheck(
        "interface Serializable {\n"
        "    func serialize() -> String;\n"
        "    func size() -> Int;\n"
        "}\n"
        "class Data : Serializable {\n"
        "    public var value: Int;\n"
        "    public func new(value: Int) -> Data {\n"
        "        return Data { value: value };\n"
        "    }\n"
        "    public func serialize() -> String {\n"
        "        return \"data\";\n"
        "    }\n"
        "    public func size() -> Int {\n"
        "        return this.value;\n"
        "    }\n"
        "}\n"
        "func main() {}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, ClassWithBaseAndInterface) {
    parseAndCheck(
        "interface Printable {\n"
        "    func display() -> String;\n"
        "}\n"
        "class Animal {\n"
        "    public var name: String;\n"
        "    public func new(name: String) -> Animal {\n"
        "        return Animal { name: name };\n"
        "    }\n"
        "}\n"
        "class Dog : Animal, Printable {\n"
        "    public func new(name: String) -> Dog {\n"
        "        return Dog { name: name };\n"
        "    }\n"
        "    public func display() -> String {\n"
        "        return this.name;\n"
        "    }\n"
        "}\n"
        "func main() {}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 24: let immutability ---

TEST_F(TypeCheckerTest, LetDeclaration) {
    parseAndCheck(
        "func main() {\n"
        "    let x: Int = 42;\n"
        "    print(x);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LetAssignmentError) {
    parseAndCheck(
        "func main() {\n"
        "    let x: Int = 42;\n"
        "    x = 10;\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, VarAssignmentOk) {
    parseAndCheck(
        "func main() {\n"
        "    var x: Int = 42;\n"
        "    x = 10;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LetTypeInference) {
    parseAndCheck(
        "func main() {\n"
        "    let name = \"Chris\";\n"
        "    let pi = 3.14;\n"
        "    print(name);\n"
        "    print(pi);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 25: Enums with associated values ---

TEST_F(TypeCheckerTest, EnumWithAssociatedValues) {
    parseAndCheck(
        "enum Result {\n"
        "    Ok(Int),\n"
        "    Error(String)\n"
        "}\n"
        "func main() {\n"
        "    var r = Result.Ok(42);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, EnumMatchDestructuring) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 26: Match as expression ---

TEST_F(TypeCheckerTest, MatchExpressionWithStrings) {
    parseAndCheck(
        "enum Color {\n"
        "    Red,\n"
        "    Green,\n"
        "    Blue\n"
        "}\n"
        "func main() {\n"
        "    var c = Color.Green;\n"
        "    var name = match c {\n"
        "        Red => \"red\"\n"
        "        Green => \"green\"\n"
        "        Blue => \"blue\"\n"
        "    };\n"
        "    print(name);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 27: Sized integer types ---

TEST_F(TypeCheckerTest, SizedIntegerTypes) {
    parseAndCheck(
        "func main() {\n"
        "    var a: Int8 = 42;\n"
        "    var b: Int16 = 1000;\n"
        "    var c: Int32 = 100000;\n"
        "    var d: UInt8 = 255;\n"
        "    var e: UInt16 = 65535;\n"
        "    var f: UInt32 = 100000;\n"
        "    var g: UInt = 42;\n"
        "    var h: Float32 = 3.14;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 29: Exhaustive match checking ---

TEST_F(TypeCheckerTest, ExhaustiveMatchAllCases) {
    parseAndCheck(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Red;\n"
        "    match c {\n"
        "        Red => print(\"red\")\n"
        "        Green => print(\"green\")\n"
        "        Blue => print(\"blue\")\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, NonExhaustiveMatchError) {
    parseAndCheck(
        "enum Color { Red, Green, Blue }\n"
        "func main() {\n"
        "    var c = Color.Red;\n"
        "    match c {\n"
        "        Red => print(\"red\")\n"
        "    }\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, NonExhaustiveMatchErrorMessage) {
    parseAndCheck(
        "enum Direction { North, South, East, West }\n"
        "func main() {\n"
        "    var d = Direction.North;\n"
        "    match d {\n"
        "        North => print(\"n\")\n"
        "        South => print(\"s\")\n"
        "    }\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

// --- Phase 48: String indexing ---

TEST_F(TypeCheckerTest, StringIndexing) {
    parseAndCheck(
        "func main() {\n"
        "    var s = \"Hello\";\n"
        "    var c = s[0];\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, StringIndexNonInt) {
    parseAndCheck(
        "func main() {\n"
        "    var s = \"Hello\";\n"
        "    var c = s[\"x\"];\n"
        "}\n"
    );
    EXPECT_TRUE(diag.hasErrors());
}

// --- Phase 46: [T] array shorthand type syntax ---

TEST_F(TypeCheckerTest, ArrayShorthandTypeSyntax) {
    parseAndCheck(
        "func sum(nums: [Int]) -> Int {\n"
        "    return 0;\n"
        "}\n"
        "func main() {\n"
        "    var data: [Int] = [1, 2, 3];\n"
        "    print(sum(data));\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 45: Array<T> function parameters ---

TEST_F(TypeCheckerTest, ArrayFunctionParam) {
    parseAndCheck(
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
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 43: Enum case keyword and class constructor ---

TEST_F(TypeCheckerTest, EnumWithCaseKeyword) {
    parseAndCheck(
        "enum Color {\n"
        "    case Red\n"
        "    case Green\n"
        "    case Blue\n"
        "}\n"
        "func main() {\n"
        "    var c = Color.Red;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, EnumWithAssociatedValuesCaseKeyword) {
    parseAndCheck(
        "enum Shape {\n"
        "    case Circle(Float)\n"
        "    case Rectangle(Float)\n"
        "}\n"
        "func main() {\n"
        "    var s = Shape.Circle(5.0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 39: If-as-expression ---

TEST_F(TypeCheckerTest, IfExpression) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 10;\n"
        "    var result = if x > 5 { \"big\" } else { \"small\" };\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, IfExpressionInt) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 10;\n"
        "    var y = if x > 5 { 1 } else { 0 };\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 40: Function type parameters ---

TEST_F(TypeCheckerTest, FunctionTypeParameter) {
    parseAndCheck(
        "func apply(x: Int, f: (Int) -> Int) -> Int {\n"
        "    return f(x);\n"
        "}\n"
        "func main() {\n"
        "    var result = apply(5, (x) => x * x);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, BlockLambdaReturnInference) {
    parseAndCheck(
        "func main() {\n"
        "    var nums = [1, 2, 3];\n"
        "    var results = nums.map((x) => {\n"
        "        var doubled = x * 2;\n"
        "        return doubled + 1;\n"
        "    });\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 38: Compound assignment operators ---

TEST_F(TypeCheckerTest, CompoundAssignmentPlus) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 10;\n"
        "    x += 5;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, CompoundAssignmentAll) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 100;\n"
        "    x += 10;\n"
        "    x -= 5;\n"
        "    x *= 2;\n"
        "    x /= 3;\n"
        "    x %= 7;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 36: Lambda type inference for array methods ---

TEST_F(TypeCheckerTest, LambdaTypeInferenceMap) {
    parseAndCheck(
        "func main() {\n"
        "    var nums = [1, 2, 3];\n"
        "    var doubled = nums.map((x) => x * 2);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, LambdaTypeInferenceFilter) {
    parseAndCheck(
        "func main() {\n"
        "    var nums = [1, 2, 3, 4];\n"
        "    var evens = nums.filter((x) => x % 2 == 0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 32: Arithmetic on sized integer types ---

TEST_F(TypeCheckerTest, SizedIntArithmetic) {
    parseAndCheck(
        "func main() {\n"
        "    var a: Int8 = 10;\n"
        "    var b: Int8 = 20;\n"
        "    var c = a + b;\n"
        "    var d: Int32 = 100;\n"
        "    var e: Int32 = 200;\n"
        "    var f = d * e;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, Float32Arithmetic) {
    parseAndCheck(
        "func main() {\n"
        "    var a: Float32 = 1.5;\n"
        "    var b: Float32 = 2.5;\n"
        "    var c = a + b;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, SizedIntComparison) {
    parseAndCheck(
        "func main() {\n"
        "    var a: Int8 = 10;\n"
        "    var b: Int8 = 20;\n"
        "    if a < b {\n"
        "        print(\"less\");\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 30: Type conversion methods for sized types ---

TEST_F(TypeCheckerTest, SizedIntConversions) {
    parseAndCheck(
        "func main() {\n"
        "    var a: Int8 = 42;\n"
        "    var s = a.toString();\n"
        "    var i = a.toInt();\n"
        "    var f = a.toFloat();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, CharConversions) {
    parseAndCheck(
        "func main() {\n"
        "    var c = 'A';\n"
        "    var s = c.toString();\n"
        "    var i = c.toInt();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, Float32Conversions) {
    parseAndCheck(
        "func main() {\n"
        "    var f: Float32 = 3.14;\n"
        "    var s = f.toString();\n"
        "    var i = f.toInt();\n"
        "    var d = f.toFloat();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, IntToChar) {
    parseAndCheck(
        "func main() {\n"
        "    var x = 65;\n"
        "    var c = x.toChar();\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Phase 28: Char type ---

TEST_F(TypeCheckerTest, CharLiteral) {
    parseAndCheck(
        "func main() {\n"
        "    var c: Char = 'A';\n"
        "    print(c);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(TypeCheckerTest, CharTypeInference) {
    parseAndCheck(
        "func main() {\n"
        "    var c = 'Z';\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Done Criterion ---

TEST_F(TypeCheckerTest, DoneCriterionProgram) {
    parseAndCheck(
        "func main() {\n"
        "    var name = \"Chris\";\n"
        "    var x: Int = 42;\n"
        "    if x > 10 {\n"
        "        print(\"Hello, ${name}! x is ${x}\");\n"
        "    }\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors()) << "Type checker rejected the done criterion program";
}
