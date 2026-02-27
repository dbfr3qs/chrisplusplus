#include <gtest/gtest.h>

extern "C" {
#include "gc.h"
}

class GCTest : public ::testing::Test {
protected:
    void SetUp() override {
        chris_gc_init();
    }
    void TearDown() override {
        chris_gc_shutdown();
    }
};

// ============================================================================
// Basic allocation tests
// ============================================================================

TEST_F(GCTest, InitAndShutdown) {
    EXPECT_EQ(chris_gc_bytes_allocated(), 0u);
    EXPECT_EQ(chris_gc_object_count(), 0u);
}

TEST_F(GCTest, AllocString) {
    void* ptr = chris_gc_alloc(32, GC_STRING);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(chris_gc_object_count(), 1u);
    EXPECT_GE(chris_gc_bytes_allocated(), 32u);
}

TEST_F(GCTest, AllocObject) {
    void* ptr = chris_gc_alloc(64, GC_OBJECT);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(chris_gc_object_count(), 1u);
}

TEST_F(GCTest, AllocArray) {
    void* ptr = chris_gc_alloc(128, GC_ARRAY);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(chris_gc_object_count(), 1u);
}

TEST_F(GCTest, MultipleAllocations) {
    for (int i = 0; i < 100; i++) {
        void* ptr = chris_gc_alloc(16, GC_STRING);
        ASSERT_NE(ptr, nullptr);
    }
    EXPECT_EQ(chris_gc_object_count(), 100u);
}

// ============================================================================
// Collection tests
// ============================================================================

TEST_F(GCTest, CollectUnrootedObjects) {
    // Allocate objects without rooting them
    for (int i = 0; i < 10; i++) {
        chris_gc_alloc(64, GC_STRING);
    }
    EXPECT_EQ(chris_gc_object_count(), 10u);

    // Force collection — all should be swept
    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 0u);
    EXPECT_EQ(chris_gc_bytes_allocated(), 0u);
}

TEST_F(GCTest, CollectPreservesRootedObjects) {
    void* ptr = chris_gc_alloc(32, GC_STRING);
    ASSERT_NE(ptr, nullptr);

    // Root the pointer
    chris_gc_push_root((void**)&ptr);

    // Also allocate some unrooted garbage
    chris_gc_alloc(32, GC_STRING);
    chris_gc_alloc(32, GC_STRING);
    EXPECT_EQ(chris_gc_object_count(), 3u);

    // Collect — only rooted object should survive
    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 1u);

    chris_gc_pop_root();
}

TEST_F(GCTest, CollectCountsIncrements) {
    EXPECT_EQ(chris_gc_total_collections(), 0u);
    chris_gc_collect();
    EXPECT_EQ(chris_gc_total_collections(), 1u);
    chris_gc_collect();
    EXPECT_EQ(chris_gc_total_collections(), 2u);
}

// ============================================================================
// Shadow stack tests
// ============================================================================

TEST_F(GCTest, PushPopRoot) {
    void* ptr = chris_gc_alloc(32, GC_STRING);
    chris_gc_push_root((void**)&ptr);
    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 1u);
    chris_gc_pop_root();
    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 0u);
}

TEST_F(GCTest, PopMultipleRoots) {
    void* a = chris_gc_alloc(16, GC_STRING);
    void* b = chris_gc_alloc(16, GC_STRING);
    void* c = chris_gc_alloc(16, GC_STRING);

    chris_gc_push_root((void**)&a);
    chris_gc_push_root((void**)&b);
    chris_gc_push_root((void**)&c);

    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 3u);

    // Pop all 3 roots at once
    chris_gc_pop_roots(3);

    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 0u);
}

TEST_F(GCTest, NestedRoots) {
    void* outer = chris_gc_alloc(32, GC_STRING);
    chris_gc_push_root((void**)&outer);

    {
        void* inner = chris_gc_alloc(32, GC_STRING);
        chris_gc_push_root((void**)&inner);

        chris_gc_collect();
        EXPECT_EQ(chris_gc_object_count(), 2u);

        chris_gc_pop_root();
    }

    // Inner is now unrooted, outer is still rooted
    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 1u);

    chris_gc_pop_root();
}

// ============================================================================
// Object with pointer fields (mark traversal)
// ============================================================================

TEST_F(GCTest, SetNumPointers) {
    // Simulate a class instance with 2 pointer fields
    void* obj = chris_gc_alloc(sizeof(void*) * 2, GC_OBJECT);
    chris_gc_set_num_pointers(obj, 2);

    GCObject* header = GC_PTR_TO_OBJ(obj);
    EXPECT_EQ(header->num_pointers, 2);
    EXPECT_EQ(header->type, GC_OBJECT);
}

TEST_F(GCTest, ObjectFieldsTracedDuringMark) {
    // Create a "parent" object with one pointer field pointing to a "child"
    void* child = chris_gc_alloc(16, GC_STRING);
    void* parent = chris_gc_alloc(sizeof(void*), GC_OBJECT);
    chris_gc_set_num_pointers(parent, 1);

    // Store child pointer in parent's first field
    *((void**)parent) = child;

    // Root only the parent
    chris_gc_push_root((void**)&parent);

    // Collect — child should survive because parent references it
    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 2u);

    chris_gc_pop_root();
}

// ============================================================================
// Finalizer tests
// ============================================================================

static int finalizer_call_count = 0;
static void test_finalizer(void* ptr) {
    (void)ptr;
    finalizer_call_count++;
}

TEST_F(GCTest, FinalizerCalledOnSweep) {
    finalizer_call_count = 0;
    chris_gc_alloc_with_finalizer(32, GC_CONTAINER, test_finalizer);
    EXPECT_EQ(chris_gc_object_count(), 1u);

    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 0u);
    EXPECT_EQ(finalizer_call_count, 1);
}

TEST_F(GCTest, FinalizerNotCalledOnRootedObject) {
    finalizer_call_count = 0;
    void* ptr = chris_gc_alloc_with_finalizer(32, GC_CONTAINER, test_finalizer);
    chris_gc_push_root((void**)&ptr);

    chris_gc_collect();
    EXPECT_EQ(finalizer_call_count, 0);

    chris_gc_pop_root();
    chris_gc_collect();
    EXPECT_EQ(finalizer_call_count, 1);
}

// ============================================================================
// Stress tests
// ============================================================================

TEST_F(GCTest, StressAllocAndCollect) {
    // Allocate many objects, keeping only a few rooted
    void* kept[10];
    for (int i = 0; i < 10; i++) {
        kept[i] = chris_gc_alloc(64, GC_STRING);
        chris_gc_push_root((void**)&kept[i]);
    }

    // Allocate a bunch of garbage
    for (int i = 0; i < 1000; i++) {
        chris_gc_alloc(64, GC_STRING);
    }

    EXPECT_EQ(chris_gc_object_count(), 1010u);

    chris_gc_collect();
    EXPECT_EQ(chris_gc_object_count(), 10u);

    chris_gc_pop_roots(10);
}

TEST_F(GCTest, RepeatedAllocCollectCycles) {
    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 100; i++) {
            chris_gc_alloc(32, GC_STRING);
        }
        chris_gc_collect();
        EXPECT_EQ(chris_gc_object_count(), 0u);
    }
    EXPECT_GE(chris_gc_total_collections(), 50u);
}

// ============================================================================
// Codegen integration tests (verify GC functions appear in generated IR)
// ============================================================================

#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/type_checker.h"
#include "common/diagnostic.h"

using namespace chris;

class GCCodegenTest : public ::testing::Test {
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

TEST_F(GCCodegenTest, MainEmitsGcInitAndShutdown) {
    auto ir = generateIR(
        "func main() {\n"
        "    print(\"hello\");\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_gc_init"), std::string::npos);
    EXPECT_NE(ir.find("chris_gc_shutdown"), std::string::npos);
}

TEST_F(GCCodegenTest, ClassAllocUsesGcAlloc) {
    auto ir = generateIR(
        "class Foo {\n"
        "    public var x: Int;\n"
        "}\n"
        "func main() {\n"
        "    var f = Foo { x: 42 };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_gc_alloc"), std::string::npos);
    // Should NOT contain raw malloc for class allocation
    // (malloc may still appear for other runtime internals, so we don't check absence)
}

TEST_F(GCCodegenTest, ConstructorMethodUsesGcAlloc) {
    auto ir = generateIR(
        "class Bar {\n"
        "    public var val: Int;\n"
        "    public func new(v: Int) -> Bar {\n"
        "        return Bar { val: v };\n"
        "    }\n"
        "}\n"
        "func main() {\n"
        "    var b = Bar.new(10);\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    EXPECT_NE(ir.find("chris_gc_alloc"), std::string::npos);
}

TEST_F(GCCodegenTest, GcSetNumPointersForObjectWithPointerFields) {
    auto ir = generateIR(
        "class Node {\n"
        "    public var name: String;\n"
        "    public var value: Int;\n"
        "}\n"
        "func main() {\n"
        "    var n = Node { name: \"test\", value: 1 };\n"
        "}\n"
    );
    ASSERT_FALSE(diag.hasErrors());
    // String field is a pointer, so set_num_pointers should be emitted
    EXPECT_NE(ir.find("chris_gc_set_num_pointers"), std::string::npos);
}
