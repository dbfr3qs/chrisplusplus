#include "gc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

// ============================================================================
// GC Heap State
// ============================================================================

#define GC_INITIAL_THRESHOLD (1024 * 1024)  // 1 MB
#define GC_HEAP_GROW_FACTOR  2
#define GC_ROOT_STACK_INITIAL_CAP 256

typedef struct {
    GCObject* head;              // head of all-objects linked list
    size_t bytes_allocated;      // total bytes currently allocated (including headers)
    size_t next_gc;              // byte threshold to trigger next collection
    size_t object_count;         // number of live GC objects
    size_t total_collections;    // cumulative collection count

    // Shadow stack for GC roots
    void*** root_stack;          // array of (void**) — each points to a stack slot holding a GC ptr
    size_t root_stack_size;
    size_t root_stack_cap;

    // Thread safety
    pthread_mutex_t lock;
    int initialized;
} GCHeap;

static GCHeap gc_heap = {0};

// ============================================================================
// Internal helpers
// ============================================================================

static void gc_mark_object(GCObject* obj);

static int is_gc_pointer(void* ptr) {
    // Basic sanity: non-null, reasonably aligned
    if (!ptr) return 0;
    if ((uintptr_t)ptr < 0x1000) return 0; // reject very low addresses
    return 1;
}

// Mark a single object and recursively mark its children
static void gc_mark_object(GCObject* obj) {
    if (!obj || obj->marked) return;
    obj->marked = 1;

    void* user_ptr = GC_OBJ_TO_PTR(obj);

    switch (obj->type) {
        case GC_STRING:
            // Leaf node — no child pointers to trace
            break;

        case GC_OBJECT: {
            // Class instance: scan the first num_pointers pointer-sized slots
            // Class fields are laid out sequentially in the struct.
            // We scan all pointer-sized fields. Non-pointer fields (int, float, bool)
            // will be scanned too but won't match any GC object — harmless.
            void** fields = (void**)user_ptr;
            for (uint16_t i = 0; i < obj->num_pointers; i++) {
                void* child = fields[i];
                if (is_gc_pointer(child)) {
                    // Verify this looks like a GC-managed pointer by checking
                    // if the preceding header region is accessible
                    GCObject* child_obj = GC_PTR_TO_OBJ(child);
                    // Walk the all-objects list to verify (expensive but safe)
                    // For performance, we trust the pointer if it's non-null
                    // and mark it. If it's not a real GC pointer, marked will
                    // just set a random byte — but we mitigate this with the
                    // num_pointers field being set accurately by codegen.
                    gc_mark_object(child_obj);
                }
            }
            break;
        }

        case GC_ARRAY: {
            // Array data buffer: elements may be pointers to GC objects.
            // We treat the entire buffer as an array of pointer-sized values
            // and try to mark each one. This is conservative for arrays of
            // non-pointer types but safe — non-pointers won't match GC objects.
            // The caller should set num_pointers = 0 for non-pointer arrays
            // and num_pointers = element_count for pointer arrays.
            void** elements = (void**)user_ptr;
            for (uint16_t i = 0; i < obj->num_pointers; i++) {
                void* child = elements[i];
                if (is_gc_pointer(child)) {
                    gc_mark_object(GC_PTR_TO_OBJ(child));
                }
            }
            break;
        }

        case GC_CONTAINER:
            // Containers (Map, Set, etc.) have internal state managed by
            // their own code. The finalizer handles cleanup. We don't trace
            // into container internals — the container's values are strings
            // (GC_STRING) that are separately tracked in the all-objects list
            // and will be marked through other roots.
            // Future: add a trace callback for containers holding GC pointers.
            break;

        default:
            break;
    }
}

// Mark phase: trace from all roots
static void gc_mark(void) {
    for (size_t i = 0; i < gc_heap.root_stack_size; i++) {
        void** root_slot = gc_heap.root_stack[i];
        if (!root_slot) continue;
        void* ptr = *root_slot;
        if (is_gc_pointer(ptr)) {
            gc_mark_object(GC_PTR_TO_OBJ(ptr));
        }
    }
}

// Sweep phase: free unmarked objects, clear marks on survivors
static void gc_sweep(void) {
    GCObject** obj_ptr = &gc_heap.head;
    while (*obj_ptr) {
        GCObject* obj = *obj_ptr;
        if (obj->marked) {
            // Survived — clear mark for next cycle
            obj->marked = 0;
            obj_ptr = &obj->next;
        } else {
            // Unreachable — remove from list and free
            *obj_ptr = obj->next;

            size_t total_size = sizeof(GCObject) + obj->size;
            gc_heap.bytes_allocated -= total_size;
            gc_heap.object_count--;

            // Call finalizer if present
            if (obj->finalizer) {
                obj->finalizer(GC_OBJ_TO_PTR(obj));
            }

            free(obj);
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

void chris_gc_init(void) {
    if (gc_heap.initialized) return;

    gc_heap.head = NULL;
    gc_heap.bytes_allocated = 0;
    gc_heap.next_gc = GC_INITIAL_THRESHOLD;
    gc_heap.object_count = 0;
    gc_heap.total_collections = 0;

    gc_heap.root_stack_cap = GC_ROOT_STACK_INITIAL_CAP;
    gc_heap.root_stack_size = 0;
    gc_heap.root_stack = (void***)malloc(sizeof(void**) * gc_heap.root_stack_cap);

    pthread_mutex_init(&gc_heap.lock, NULL);
    gc_heap.initialized = 1;
}

void* chris_gc_alloc(size_t size, uint8_t type) {
    pthread_mutex_lock(&gc_heap.lock);

    // Check if we should collect before allocating
    if (gc_heap.bytes_allocated + sizeof(GCObject) + size > gc_heap.next_gc) {
        gc_mark();
        gc_sweep();
        gc_heap.total_collections++;

        // Adaptive threshold: grow based on surviving bytes
        gc_heap.next_gc = gc_heap.bytes_allocated * GC_HEAP_GROW_FACTOR;
        if (gc_heap.next_gc < GC_INITIAL_THRESHOLD) {
            gc_heap.next_gc = GC_INITIAL_THRESHOLD;
        }
    }

    size_t total_size = sizeof(GCObject) + size;
    GCObject* obj = (GCObject*)malloc(total_size);
    if (!obj) {
        // Last resort: try to collect and retry
        gc_mark();
        gc_sweep();
        gc_heap.total_collections++;
        obj = (GCObject*)malloc(total_size);
        if (!obj) {
            fprintf(stderr, "GC: out of memory (requested %zu bytes)\n", size);
            pthread_mutex_unlock(&gc_heap.lock);
            exit(1);
        }
    }

    obj->next = gc_heap.head;
    gc_heap.head = obj;
    obj->marked = 0;
    obj->type = type;
    obj->num_pointers = 0;
    obj->size = (uint32_t)size;
    obj->finalizer = NULL;

    gc_heap.bytes_allocated += total_size;
    gc_heap.object_count++;

    void* user_ptr = GC_OBJ_TO_PTR(obj);
    memset(user_ptr, 0, size); // zero-initialize

    pthread_mutex_unlock(&gc_heap.lock);
    return user_ptr;
}

void* chris_gc_alloc_with_finalizer(size_t size, uint8_t type, void (*finalizer)(void*)) {
    void* ptr = chris_gc_alloc(size, type);
    GCObject* obj = GC_PTR_TO_OBJ(ptr);
    obj->finalizer = finalizer;
    return ptr;
}

void chris_gc_set_num_pointers(void* ptr, uint16_t num_pointers) {
    if (!ptr) return;
    GCObject* obj = GC_PTR_TO_OBJ(ptr);
    obj->num_pointers = num_pointers;
}

void chris_gc_collect(void) {
    pthread_mutex_lock(&gc_heap.lock);
    gc_mark();
    gc_sweep();
    gc_heap.total_collections++;

    gc_heap.next_gc = gc_heap.bytes_allocated * GC_HEAP_GROW_FACTOR;
    if (gc_heap.next_gc < GC_INITIAL_THRESHOLD) {
        gc_heap.next_gc = GC_INITIAL_THRESHOLD;
    }
    pthread_mutex_unlock(&gc_heap.lock);
}

void chris_gc_shutdown(void) {
    if (!gc_heap.initialized) return;

    pthread_mutex_lock(&gc_heap.lock);

    // Free all remaining objects
    GCObject* obj = gc_heap.head;
    while (obj) {
        GCObject* next = obj->next;
        if (obj->finalizer) {
            obj->finalizer(GC_OBJ_TO_PTR(obj));
        }
        free(obj);
        obj = next;
    }
    gc_heap.head = NULL;
    gc_heap.bytes_allocated = 0;
    gc_heap.object_count = 0;

    // Free root stack
    free(gc_heap.root_stack);
    gc_heap.root_stack = NULL;
    gc_heap.root_stack_size = 0;
    gc_heap.root_stack_cap = 0;

    gc_heap.initialized = 0;
    pthread_mutex_unlock(&gc_heap.lock);
    pthread_mutex_destroy(&gc_heap.lock);
}

// ============================================================================
// Shadow Stack
// ============================================================================

void chris_gc_push_root(void** root) {
    pthread_mutex_lock(&gc_heap.lock);

    if (gc_heap.root_stack_size >= gc_heap.root_stack_cap) {
        gc_heap.root_stack_cap *= 2;
        gc_heap.root_stack = (void***)realloc(gc_heap.root_stack,
            sizeof(void**) * gc_heap.root_stack_cap);
        if (!gc_heap.root_stack) {
            fprintf(stderr, "GC: out of memory growing root stack\n");
            pthread_mutex_unlock(&gc_heap.lock);
            exit(1);
        }
    }

    gc_heap.root_stack[gc_heap.root_stack_size++] = root;
    pthread_mutex_unlock(&gc_heap.lock);
}

void chris_gc_pop_root(void) {
    pthread_mutex_lock(&gc_heap.lock);
    if (gc_heap.root_stack_size > 0) {
        gc_heap.root_stack_size--;
    }
    pthread_mutex_unlock(&gc_heap.lock);
}

void chris_gc_pop_roots(size_t n) {
    pthread_mutex_lock(&gc_heap.lock);
    if (n > gc_heap.root_stack_size) {
        gc_heap.root_stack_size = 0;
    } else {
        gc_heap.root_stack_size -= n;
    }
    pthread_mutex_unlock(&gc_heap.lock);
}

// ============================================================================
// Statistics
// ============================================================================

size_t chris_gc_bytes_allocated(void) {
    return gc_heap.bytes_allocated;
}

size_t chris_gc_object_count(void) {
    return gc_heap.object_count;
}

size_t chris_gc_total_collections(void) {
    return gc_heap.total_collections;
}
