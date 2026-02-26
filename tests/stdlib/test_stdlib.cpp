#include <gtest/gtest.h>
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "sema/type_checker.h"
#include "codegen/codegen.h"
#include "common/diagnostic.h"

using namespace chris;

// ============================================================================
// Type Checker Tests for Math Built-ins
// ============================================================================

class StdlibTypeCheckerTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    Program parseAndCheck(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        TypeChecker checker(diag);
        checker.check(program);
        return program;
    }
};

// --- Math (Int) ---

TEST_F(StdlibTypeCheckerTest, AbsFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    return abs(-5);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, MinFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    return min(3, 7);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, MaxFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    return max(3, 7);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, RandomFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    return random(1, 100);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Math (Float) ---

TEST_F(StdlibTypeCheckerTest, SqrtFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return sqrt(16.0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, PowFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return pow(2.0, 10.0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, FloorFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return floor(3.7);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, CeilFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return ceil(3.2);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, RoundFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return round(3.5);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, LogFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return log(2.718);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, SinFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return sin(0.0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, CosFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return cos(0.0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, TanFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return tan(0.0);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, FabsFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return fabs(-3.14);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, FminFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return fmin(1.5, 2.5);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, FmaxFunction) {
    parseAndCheck(
        "func main() -> Float {\n"
        "    return fmax(1.5, 2.5);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Stdin ---

TEST_F(StdlibTypeCheckerTest, ReadLineFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var line: String = readLine();\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// --- Test Assertions ---

TEST_F(StdlibTypeCheckerTest, AssertFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    assert(true, \"should be true\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, AssertTrueFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    assertTrue(1 == 1, \"1 equals 1\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, AssertFalseFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    assertFalse(1 == 2, \"1 != 2\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, AssertEqualIntFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    assertEqual(42, 42, \"should match\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, AssertEqualStringFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    assertEqual(\"hello\", \"hello\", \"strings match\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// Codegen Tests for Math Built-ins
// ============================================================================

class StdlibCodegenTest : public ::testing::Test {
protected:
    DiagnosticEngine diag;

    bool compiles(const std::string& source) {
        Lexer lexer(source, "test.chr", diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto program = parser.parse();
        if (diag.hasErrors()) return false;

        TypeChecker checker(diag);
        checker.check(program);
        if (diag.hasErrors()) return false;

        CodeGen codegen("test.chr", diag);
        return codegen.generate(program, checker.genericInstantiations());
    }
};

TEST_F(StdlibCodegenTest, MathIntFunctions) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var a = abs(-10);\n"
        "    var b = min(1, 2);\n"
        "    var c = max(1, 2);\n"
        "    var d = random(1, 100);\n"
        "    return a + b + c + d;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, MathFloatFunctions) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var a = sqrt(4.0);\n"
        "    var b = pow(2.0, 3.0);\n"
        "    var c = floor(3.7);\n"
        "    var d = ceil(3.2);\n"
        "    var e = round(3.5);\n"
        "    var f = log(1.0);\n"
        "    var g = sin(0.0);\n"
        "    var h = cos(0.0);\n"
        "    var i = tan(0.0);\n"
        "    var j = fabs(-1.0);\n"
        "    var k = fmin(1.0, 2.0);\n"
        "    var l = fmax(1.0, 2.0);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, ReadLineCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var line = readLine();\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, AssertionsCompile) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    assert(true, \"ok\");\n"
        "    assertTrue(1 == 1, \"ok\");\n"
        "    assertFalse(1 == 2, \"ok\");\n"
        "    assertEqual(42, 42, \"ok\");\n"
        "    assertEqual(\"a\", \"a\", \"ok\");\n"
        "    return 0;\n"
        "}\n"
    ));
}

// ============================================================================
// JSON Type Checker Tests
// ============================================================================

TEST_F(StdlibTypeCheckerTest, JsonParseFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{}\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonGetFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"name\\\": \\\"Alice\\\"}\");\n"
        "    var name: String = jsonGet(obj, \"name\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonGetIntFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"age\\\": 30}\");\n"
        "    return jsonGetInt(obj, \"age\");\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonGetBoolFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"active\\\": true}\");\n"
        "    var active: Bool = jsonGetBool(obj, \"active\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonGetFloatFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"pi\\\": 3.14}\");\n"
        "    var pi: Float = jsonGetFloat(obj, \"pi\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonGetArrayFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"items\\\": [1,2,3]}\");\n"
        "    var arr = jsonGetArray(obj, \"items\");\n"
        "    return jsonArrayLength(arr);\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonGetObjectFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"user\\\": {\\\"id\\\": 1}}\");\n"
        "    var user = jsonGetObject(obj, \"user\");\n"
        "    return jsonGetInt(user, \"id\");\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, JsonStringifyFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"a\\\": 1}\");\n"
        "    var s: String = jsonStringify(obj);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// JSON Codegen Tests
// ============================================================================

TEST_F(StdlibCodegenTest, JsonCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"name\\\": \\\"Alice\\\", \\\"age\\\": 30}\");\n"
        "    var name = jsonGet(obj, \"name\");\n"
        "    var age = jsonGetInt(obj, \"age\");\n"
        "    var s = jsonStringify(obj);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, JsonNestedCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var obj = jsonParse(\"{\\\"user\\\": {\\\"id\\\": 42}}\");\n"
        "    var user = jsonGetObject(obj, \"user\");\n"
        "    var id = jsonGetInt(user, \"id\");\n"
        "    var arr = jsonParse(\"[1,2,3]\");\n"
        "    return 0;\n"
        "}\n"
    ));
}

// ============================================================================
// Set Type Checker Tests
// ============================================================================

TEST_F(StdlibTypeCheckerTest, SetConstructor) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, SetAddMethod) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    s.add(\"hello\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, SetHasMethod) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    s.add(\"hello\");\n"
        "    var exists: Bool = s.has(\"hello\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, SetRemoveMethod) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    s.add(\"hello\");\n"
        "    var removed: Bool = s.remove(\"hello\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, SetSizeProperty) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    return s.size;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, SetClearMethod) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    s.clear();\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// Set Codegen Tests
// ============================================================================

TEST_F(StdlibCodegenTest, SetBasicCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    s.add(\"apple\");\n"
        "    s.add(\"banana\");\n"
        "    return s.size;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, SetMethodsCompile) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var s = Set();\n"
        "    s.add(\"x\");\n"
        "    s.has(\"x\");\n"
        "    s.remove(\"x\");\n"
        "    s.clear();\n"
        "    return 0;\n"
        "}\n"
    ));
}

// ============================================================================
// Networking Type Checker Tests
// ============================================================================

TEST_F(StdlibTypeCheckerTest, TcpConnectFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var fd: Int = tcpConnect(\"127.0.0.1\", 8080);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, TcpListenAcceptFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var server: Int = tcpListen(8080);\n"
        "    var client: Int = tcpAccept(server);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, TcpSendRecvFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var sent: Int = tcpSend(3, \"hello\");\n"
        "    var data: String = tcpRecv(3, 1024);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, TcpCloseFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    tcpClose(3);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, UdpCreateBindFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var fd: Int = udpCreate();\n"
        "    var result: Int = udpBind(fd, 9090);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, UdpSendRecvFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var fd = udpCreate();\n"
        "    var sent: Int = udpSendTo(fd, \"127.0.0.1\", 9090, \"hello\");\n"
        "    var data: String = udpRecvFrom(fd, 1024);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, UdpCloseFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var fd = udpCreate();\n"
        "    udpClose(fd);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, DnsLookupFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var ip: String = dnsLookup(\"localhost\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// Networking Codegen Tests
// ============================================================================

TEST_F(StdlibCodegenTest, TcpCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var fd = tcpConnect(\"127.0.0.1\", 80);\n"
        "    tcpSend(fd, \"GET / HTTP/1.0\\r\\n\\r\\n\");\n"
        "    var resp = tcpRecv(fd, 4096);\n"
        "    tcpClose(fd);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, TcpServerCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var server = tcpListen(8080);\n"
        "    var client = tcpAccept(server);\n"
        "    tcpSend(client, \"hello\");\n"
        "    tcpClose(client);\n"
        "    tcpClose(server);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, UdpCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var fd = udpCreate();\n"
        "    udpBind(fd, 9090);\n"
        "    udpSendTo(fd, \"127.0.0.1\", 9090, \"ping\");\n"
        "    var data = udpRecvFrom(fd, 1024);\n"
        "    udpClose(fd);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, DnsCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var ip = dnsLookup(\"localhost\");\n"
        "    return 0;\n"
        "}\n"
    ));
}

// ============================================================================
// HTTP Type Checker Tests
// ============================================================================

TEST_F(StdlibTypeCheckerTest, HttpGetFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var body: String = httpGet(\"http://example.com\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, HttpPostFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var resp: String = httpPost(\"http://example.com/api\", \"{}\", \"application/json\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, HttpServerCreateFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var server: Int = httpServerCreate(8080);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, HttpServerAcceptFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var server = httpServerCreate(8080);\n"
        "    var req: Int = httpServerAccept(server);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, HttpRequestAccessors) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var method: String = httpRequestMethod(1);\n"
        "    var path: String = httpRequestPath(1);\n"
        "    var body: String = httpRequestBody(1);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, HttpRespondFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    httpRespond(1, 200, \"OK\");\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, HttpServerCloseFunction) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    httpServerClose(1);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// HTTP Codegen Tests
// ============================================================================

TEST_F(StdlibCodegenTest, HttpClientCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var body = httpGet(\"http://example.com\");\n"
        "    var resp = httpPost(\"http://example.com/api\", \"{}\", \"application/json\");\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, HttpServerCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var server = httpServerCreate(8080);\n"
        "    var req = httpServerAccept(server);\n"
        "    var method = httpRequestMethod(req);\n"
        "    var path = httpRequestPath(req);\n"
        "    var body = httpRequestBody(req);\n"
        "    httpRespond(req, 200, \"Hello\");\n"
        "    httpServerClose(server);\n"
        "    return 0;\n"
        "}\n"
    ));
}

// ============================================================================
// Concurrent Type Checker Tests
// ============================================================================

TEST_F(StdlibTypeCheckerTest, ConcurrentMapFunctions) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var cm: Int = ConcurrentMap();\n"
        "    cmapSet(cm, \"key\", 42);\n"
        "    var val: Int = cmapGet(cm, \"key\");\n"
        "    var exists: Bool = cmapHas(cm, \"key\");\n"
        "    var deleted: Bool = cmapDelete(cm, \"key\");\n"
        "    var sz: Int = cmapSize(cm);\n"
        "    cmapDestroy(cm);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, ConcurrentQueueFunctions) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var q: Int = ConcurrentQueue();\n"
        "    cqueueEnqueue(q, 10);\n"
        "    var val: Int = cqueueDequeue(q);\n"
        "    var sz: Int = cqueueSize(q);\n"
        "    var empty: Bool = cqueueIsEmpty(q);\n"
        "    cqueueDestroy(q);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

TEST_F(StdlibTypeCheckerTest, AtomicFunctions) {
    parseAndCheck(
        "func main() -> Int {\n"
        "    var a: Int = atomicCreate(0);\n"
        "    var val: Int = atomicLoad(a);\n"
        "    atomicStore(a, 42);\n"
        "    var sum: Int = atomicAdd(a, 10);\n"
        "    var diff: Int = atomicSub(a, 5);\n"
        "    var ok: Bool = atomicCompareSwap(a, 47, 100);\n"
        "    atomicDestroy(a);\n"
        "    return 0;\n"
        "}\n"
    );
    EXPECT_FALSE(diag.hasErrors());
}

// ============================================================================
// Concurrent Codegen Tests
// ============================================================================

TEST_F(StdlibCodegenTest, ConcurrentMapCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var cm = ConcurrentMap();\n"
        "    cmapSet(cm, \"a\", 1);\n"
        "    cmapGet(cm, \"a\");\n"
        "    cmapHas(cm, \"a\");\n"
        "    cmapDelete(cm, \"a\");\n"
        "    cmapSize(cm);\n"
        "    cmapDestroy(cm);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, ConcurrentQueueCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var q = ConcurrentQueue();\n"
        "    cqueueEnqueue(q, 42);\n"
        "    cqueueDequeue(q);\n"
        "    cqueueSize(q);\n"
        "    cqueueIsEmpty(q);\n"
        "    cqueueDestroy(q);\n"
        "    return 0;\n"
        "}\n"
    ));
}

TEST_F(StdlibCodegenTest, AtomicCompiles) {
    EXPECT_TRUE(compiles(
        "func main() -> Int {\n"
        "    var a = atomicCreate(0);\n"
        "    atomicLoad(a);\n"
        "    atomicStore(a, 10);\n"
        "    atomicAdd(a, 5);\n"
        "    atomicSub(a, 3);\n"
        "    atomicCompareSwap(a, 12, 99);\n"
        "    atomicDestroy(a);\n"
        "    return 0;\n"
        "}\n"
    ));
}
