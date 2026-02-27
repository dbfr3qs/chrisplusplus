#ifndef CHRIS_GC_H
#define CHRIS_GC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// GC object type tags
#define GC_STRING    0
#define GC_OBJECT    1
#define GC_ARRAY     2
#define GC_CONTAINER 3

// Object header prepended to every GC-managed allocation
typedef struct GCObject {
    struct GCObject* next;    // intrusive linked list of all GC objects
    uint8_t marked;           // mark bit
    uint8_t type;             // GC_STRING, GC_OBJECT, GC_ARRAY, GC_CONTAINER
    uint16_t num_pointers;    // number of pointer-typed fields (for mark traversal)
    uint32_t size;            // allocation size (excluding header)
    void (*finalizer)(void*); // optional finalizer (for containers with internal malloc'd state)
} GCObject;

// Get the user-visible pointer from a GCObject header
#define GC_OBJ_TO_PTR(obj)  ((void*)((char*)(obj) + sizeof(GCObject)))
// Get the GCObject header from a user-visible pointer
#define GC_PTR_TO_OBJ(ptr)  ((GCObject*)((char*)(ptr) - sizeof(GCObject)))

// ============================================================================
// Public API
// ============================================================================

// Initialize the GC heap. Must be called once at program start.
void chris_gc_init(void);

// Allocate a GC-managed block of `size` bytes with the given type tag.
// Returns a pointer to the usable memory (after the header).
void* chris_gc_alloc(size_t size, uint8_t type);

// Allocate a GC-managed block with a finalizer callback.
// The finalizer is called with the user pointer before the object is freed.
void* chris_gc_alloc_with_finalizer(size_t size, uint8_t type, void (*finalizer)(void*));

// Set the number of pointer fields in an object (for mark traversal of class instances).
// Must be called immediately after chris_gc_alloc for GC_OBJECT allocations.
void chris_gc_set_num_pointers(void* ptr, uint16_t num_pointers);

// Run a full mark-and-sweep collection.
void chris_gc_collect(void);

// Shut down the GC, freeing all remaining objects. Called at program exit.
void chris_gc_shutdown(void);

// ============================================================================
// Shadow Stack (root management)
// ============================================================================

// Push a pointer-to-pointer as a GC root. The root points to a stack slot
// that holds a GC-managed pointer. The GC will dereference it during marking.
void chris_gc_push_root(void** root);

// Pop the most recently pushed root.
void chris_gc_pop_root(void);

// Pop N roots at once (used at function return).
void chris_gc_pop_roots(size_t n);

// ============================================================================
// Statistics (for testing/debugging)
// ============================================================================

size_t chris_gc_bytes_allocated(void);
size_t chris_gc_object_count(void);
size_t chris_gc_total_collections(void);

#ifdef __cplusplus
}
#endif

#endif // CHRIS_GC_H
