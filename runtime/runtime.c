#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <pthread.h>

// Exception handling support
#define CHRIS_MAX_TRY_DEPTH 64

static jmp_buf chris_try_stack[CHRIS_MAX_TRY_DEPTH];
static int chris_try_depth = 0;
static const char* chris_exception_message = NULL;

int chris_try_begin(void) {
    if (chris_try_depth >= CHRIS_MAX_TRY_DEPTH) {
        fprintf(stderr, "Error: try nesting too deep\n");
        exit(1);
    }
    return chris_try_depth++;
}

void chris_try_end(void) {
    if (chris_try_depth > 0) chris_try_depth--;
}

void chris_throw(const char* message) {
    chris_exception_message = message;
    if (chris_try_depth > 0) {
        chris_try_depth--;
        longjmp(chris_try_stack[chris_try_depth], 1);
    } else {
        fprintf(stderr, "Unhandled exception: %s\n", message ? message : "(nil)");
        exit(1);
    }
}

const char* chris_get_exception(void) {
    return chris_exception_message;
}

jmp_buf* chris_get_jmpbuf(int depth) {
    return &chris_try_stack[depth];
}

void chris_print(const char* str) {
    if (str) {
        printf("%s\n", str);
    } else {
        printf("nil\n");
    }
}

const char* chris_strcat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char* result = (char*)malloc(len_a + len_b + 1);
    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b);
    result[len_a + len_b] = '\0';
    return result;
}

const char* chris_char_to_str(char c) {
    char* buf = (char*)malloc(2);
    buf[0] = c;
    buf[1] = '\0';
    return buf;
}

const char* chris_int_to_str(long long val) {
    char* buf = (char*)malloc(32);
    snprintf(buf, 32, "%lld", val);
    return buf;
}

const char* chris_float_to_str(double val) {
    char* buf = (char*)malloc(64);
    snprintf(buf, 64, "%g", val);
    return buf;
}

const char* chris_bool_to_str(int val) {
    return val ? "true" : "false";
}

// Array support
void* chris_array_alloc(long long elem_size, long long count) {
    return malloc((size_t)(elem_size * count));
}

void chris_array_bounds_check(long long index, long long length) {
    if (index < 0 || index >= length) {
        fprintf(stderr, "Array index out of bounds: index %lld, length %lld\n", index, length);
        exit(1);
    }
}

// String conversion methods
long long chris_str_to_int(const char* str) {
    if (!str) return 0;
    return atoll(str);
}

double chris_str_to_float(const char* str) {
    if (!str) return 0.0;
    return atof(str);
}

long long chris_str_len(const char* str) {
    if (!str) return 0;
    return (long long)strlen(str);
}

// String methods
int chris_str_contains(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    return strstr(str, substr) != NULL;
}

int chris_str_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    size_t plen = strlen(prefix);
    return strncmp(str, prefix, plen) == 0;
}

int chris_str_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t slen = strlen(str);
    size_t xlen = strlen(suffix);
    if (xlen > slen) return 0;
    return strcmp(str + slen - xlen, suffix) == 0;
}

long long chris_str_index_of(const char* str, const char* substr) {
    if (!str || !substr) return -1;
    const char* found = strstr(str, substr);
    if (!found) return -1;
    return (long long)(found - str);
}

const char* chris_str_substring(const char* str, long long start, long long end) {
    if (!str) return "";
    size_t len = strlen(str);
    if (start < 0) start = 0;
    if (end < start) end = start;
    if ((size_t)end > len) end = (long long)len;
    size_t sublen = (size_t)(end - start);
    char* result = (char*)malloc(sublen + 1);
    memcpy(result, str + start, sublen);
    result[sublen] = '\0';
    return result;
}

const char* chris_str_replace(const char* str, const char* old_s, const char* new_s) {
    if (!str || !old_s || !new_s) return str ? str : "";
    size_t old_len = strlen(old_s);
    size_t new_len = strlen(new_s);
    if (old_len == 0) return str;

    // Count occurrences
    int count = 0;
    const char* p = str;
    while ((p = strstr(p, old_s)) != NULL) { count++; p += old_len; }

    size_t result_len = strlen(str) + count * ((long long)new_len - (long long)old_len);
    char* result = (char*)malloc(result_len + 1);
    char* dst = result;
    p = str;
    while (*p) {
        if (strncmp(p, old_s, old_len) == 0) {
            memcpy(dst, new_s, new_len);
            dst += new_len;
            p += old_len;
        } else {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
}

const char* chris_str_trim(const char* str) {
    if (!str) return "";
    while (*str && isspace((unsigned char)*str)) str++;
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) len--;
    char* result = (char*)malloc(len + 1);
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

const char* chris_str_to_upper(const char* str) {
    if (!str) return "";
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; i++) result[i] = (char)toupper((unsigned char)str[i]);
    result[len] = '\0';
    return result;
}

const char* chris_str_to_lower(const char* str) {
    if (!str) return "";
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; i++) result[i] = (char)tolower((unsigned char)str[i]);
    result[len] = '\0';
    return result;
}

const char* chris_str_char_at(const char* str, long long index) {
    if (!str || index < 0 || (size_t)index >= strlen(str)) return "";
    char* result = (char*)malloc(2);
    result[0] = str[index];
    result[1] = '\0';
    return result;
}

// Array struct: {i64 length, ptr data}
typedef struct { long long length; void* data; } ChrisArray;

// Array methods
void chris_array_push(ChrisArray* arr, long long elem_size, long long value) {
    long long new_len = arr->length + 1;
    void* new_data = realloc(arr->data, (size_t)(elem_size * new_len));
    if (!new_data) { fprintf(stderr, "Out of memory in array push\n"); exit(1); }
    arr->data = new_data;
    // Store value at the end
    memcpy((char*)arr->data + elem_size * arr->length, &value, (size_t)elem_size);
    arr->length = new_len;
}

long long chris_array_pop(ChrisArray* arr, long long elem_size) {
    if (arr->length <= 0) {
        fprintf(stderr, "Array pop on empty array\n");
        exit(1);
    }
    arr->length--;
    long long value = 0;
    memcpy(&value, (char*)arr->data + elem_size * arr->length, (size_t)elem_size);
    return value;
}

void chris_array_reverse(ChrisArray* arr, long long elem_size) {
    if (arr->length <= 1) return;
    char* data = (char*)arr->data;
    char tmp[16]; // enough for i64 or double or pointer
    for (long long i = 0; i < arr->length / 2; i++) {
        long long j = arr->length - 1 - i;
        memcpy(tmp, data + i * elem_size, (size_t)elem_size);
        memcpy(data + i * elem_size, data + j * elem_size, (size_t)elem_size);
        memcpy(data + j * elem_size, tmp, (size_t)elem_size);
    }
}

const char* chris_array_join(ChrisArray* arr, const char* sep) {
    if (!arr || arr->length == 0) {
        char* empty = (char*)malloc(1);
        empty[0] = '\0';
        return empty;
    }
    // Assumes array of strings (ptr elements)
    const char** strings = (const char**)arr->data;
    size_t sep_len = sep ? strlen(sep) : 0;
    size_t total = 0;
    for (long long i = 0; i < arr->length; i++) {
        if (strings[i]) total += strlen(strings[i]);
        if (i < arr->length - 1) total += sep_len;
    }
    char* result = (char*)malloc(total + 1);
    char* dst = result;
    for (long long i = 0; i < arr->length; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]);
            memcpy(dst, strings[i], len);
            dst += len;
        }
        if (i < arr->length - 1 && sep) {
            memcpy(dst, sep, sep_len);
            dst += sep_len;
        }
    }
    *dst = '\0';
    return result;
}

// Callback type for map: i64 -> i64
typedef long long (*MapCallback)(long long);
typedef int (*FilterCallback)(long long);
typedef void (*ForEachCallback)(long long);

void chris_array_map(ChrisArray* arr, long long elem_size, MapCallback cb, ChrisArray* out) {
    out->length = arr->length;
    out->data = malloc((size_t)(elem_size * arr->length));
    long long* src = (long long*)arr->data;
    long long* dst = (long long*)out->data;
    for (long long i = 0; i < arr->length; i++) {
        dst[i] = cb(src[i]);
    }
}

void chris_array_filter(ChrisArray* arr, long long elem_size, FilterCallback cb, ChrisArray* out) {
    // Allocate max possible size
    long long* src = (long long*)arr->data;
    long long* tmp = (long long*)malloc((size_t)(elem_size * arr->length));
    long long count = 0;
    for (long long i = 0; i < arr->length; i++) {
        if (cb(src[i])) {
            tmp[count++] = src[i];
        }
    }
    out->length = count;
    out->data = tmp;
}

void chris_array_foreach(ChrisArray* arr, long long elem_size, ForEachCallback cb) {
    long long* src = (long long*)arr->data;
    for (long long i = 0; i < arr->length; i++) {
        cb(src[i]);
    }
}

void chris_str_split(const char* str, const char* delim, ChrisArray* out) {
    if (!str || !delim || !out) {
        if (out) { out->length = 0; out->data = NULL; }
        return;
    }
    size_t dlen = strlen(delim);
    if (dlen == 0) {
        // Split into individual characters
        size_t slen = strlen(str);
        const char** parts = (const char**)malloc(sizeof(const char*) * slen);
        for (size_t i = 0; i < slen; i++) {
            char* ch = (char*)malloc(2);
            ch[0] = str[i];
            ch[1] = '\0';
            parts[i] = ch;
        }
        out->length = (long long)slen;
        out->data = parts;
        return;
    }

    // Count parts
    int count = 1;
    const char* p = str;
    while ((p = strstr(p, delim)) != NULL) { count++; p += dlen; }

    const char** parts = (const char**)malloc(sizeof(const char*) * count);
    p = str;
    for (int i = 0; i < count; i++) {
        const char* next = strstr(p, delim);
        size_t partlen = next ? (size_t)(next - p) : strlen(p);
        char* part = (char*)malloc(partlen + 1);
        memcpy(part, p, partlen);
        part[partlen] = '\0';
        parts[i] = part;
        if (next) p = next + dlen; else break;
    }
    out->length = (long long)count;
    out->data = parts;
}

// ============================================================================
// Test Runtime Support
// ============================================================================

static int chris_test_passed = 0;
static int chris_test_failed = 0;

void chris_test_start(const char* name) {
    printf("  [ RUN  ] %s\n", name);
}

void chris_test_pass(const char* name) {
    printf("  [  OK  ] %s\n", name);
    chris_test_passed++;
}

void chris_test_fail(const char* name) {
    printf("  [ FAIL ] %s\n", name);
    chris_test_failed++;
}

// Returns the number of failed tests (used as exit code)
int chris_test_summary(void) {
    int total = chris_test_passed + chris_test_failed;
    printf("\n%d test(s) ran: %d passed, %d failed.\n", total, chris_test_passed, chris_test_failed);
    return chris_test_failed;
}

// assert_eq(a, b) — fails the current test if a != b
void chris_assert_eq(long long a, long long b, const char* expr) {
    if (a != b) {
        printf("    ASSERTION FAILED: %s (expected %lld, got %lld)\n", expr, b, a);
        chris_throw("assertion failed");
    }
}

// assert_true(cond) — fails the current test if cond is false
void chris_assert_true(long long cond, const char* expr) {
    if (!cond) {
        printf("    ASSERTION FAILED: %s (expected true)\n", expr);
        chris_throw("assertion failed");
    }
}

// ============================================================================
// Shared Class Mutex Support
// ============================================================================

// Initialize a pthread mutex at the given pointer
void chris_mutex_init(void* ptr) {
    pthread_mutex_init((pthread_mutex_t*)ptr, NULL);
}

// Lock a pthread mutex
void chris_mutex_lock(void* ptr) {
    pthread_mutex_lock((pthread_mutex_t*)ptr);
}

// Unlock a pthread mutex
void chris_mutex_unlock(void* ptr) {
    pthread_mutex_unlock((pthread_mutex_t*)ptr);
}

// Destroy a pthread mutex
void chris_mutex_destroy(void* ptr) {
    pthread_mutex_destroy((pthread_mutex_t*)ptr);
}

// ============================================================================
// Channel Runtime Support
// ============================================================================

#define CHRIS_CHANNEL_MAX_BUFFER 4096

typedef struct chris_channel {
    long long* buffer;
    int capacity;
    int count;
    int head;       // read index
    int tail;       // write index
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} chris_channel;

// Create a new channel with the given buffer capacity
chris_channel* chris_channel_create(long long capacity) {
    if (capacity <= 0) capacity = 1;
    if (capacity > CHRIS_CHANNEL_MAX_BUFFER) capacity = CHRIS_CHANNEL_MAX_BUFFER;
    chris_channel* ch = (chris_channel*)malloc(sizeof(chris_channel));
    ch->buffer = (long long*)malloc(sizeof(long long) * capacity);
    ch->capacity = (int)capacity;
    ch->count = 0;
    ch->head = 0;
    ch->tail = 0;
    ch->closed = 0;
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    return ch;
}

// Send a value into the channel (blocks if full, fails if closed)
// Returns 1 on success, 0 if channel is closed
int chris_channel_send(chris_channel* ch, long long value) {
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == ch->capacity && !ch->closed) {
        pthread_cond_wait(&ch->not_full, &ch->mutex);
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return 0;
    }
    ch->buffer[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return 1;
}

// Receive a value from the channel (blocks if empty)
// Returns 1 on success (value written to *out), 0 if channel is closed and empty
int chris_channel_recv(chris_channel* ch, long long* out) {
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed) {
        pthread_cond_wait(&ch->not_empty, &ch->mutex);
    }
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return 0;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mutex);
    return 1;
}

// Close the channel — no more sends allowed, pending receives drain remaining values
void chris_channel_close(chris_channel* ch) {
    pthread_mutex_lock(&ch->mutex);
    ch->closed = 1;
    pthread_cond_broadcast(&ch->not_empty);
    pthread_cond_broadcast(&ch->not_full);
    pthread_mutex_unlock(&ch->mutex);
}

// Destroy the channel and free resources
void chris_channel_destroy(chris_channel* ch) {
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->not_empty);
    pthread_cond_destroy(&ch->not_full);
    free(ch->buffer);
    free(ch);
}

// ============================================================================
// Async/Await Runtime Support
// ============================================================================

// Async task kind
#define CHRIS_ASYNC_IO      0
#define CHRIS_ASYNC_COMPUTE 1

// Task state
#define CHRIS_TASK_PENDING   0
#define CHRIS_TASK_RUNNING   1
#define CHRIS_TASK_COMPLETED 2

// Thunk function type: takes void* args, returns i64 result
typedef long long (*chris_thunk_fn)(void*);

// Future/Task structure
typedef struct chris_future {
    chris_thunk_fn func;       // the thunk function to execute
    void*          args;       // packed arguments pointer
    int            kind;       // CHRIS_ASYNC_IO or CHRIS_ASYNC_COMPUTE
    int            state;      // CHRIS_TASK_PENDING/RUNNING/COMPLETED
    long long      result;     // return value (valid when state == COMPLETED)
    pthread_t      thread;     // thread handle
    pthread_mutex_t mutex;     // protects state and result
    pthread_cond_t  cond;      // signaled when task completes
} chris_future;

// Global task registry for run_loop
#define CHRIS_MAX_TASKS 1024
static chris_future* chris_task_registry[CHRIS_MAX_TASKS];
static int chris_task_count = 0;
static pthread_mutex_t chris_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

static void chris_register_task(chris_future* f) {
    pthread_mutex_lock(&chris_registry_mutex);
    if (chris_task_count < CHRIS_MAX_TASKS) {
        chris_task_registry[chris_task_count++] = f;
    }
    pthread_mutex_unlock(&chris_registry_mutex);
}

// Thread entry point
static void* chris_async_thread_entry(void* arg) {
    chris_future* f = (chris_future*)arg;

    pthread_mutex_lock(&f->mutex);
    f->state = CHRIS_TASK_RUNNING;
    pthread_mutex_unlock(&f->mutex);

    // Execute the thunk
    long long res = f->func(f->args);

    pthread_mutex_lock(&f->mutex);
    f->result = res;
    f->state = CHRIS_TASK_COMPLETED;
    pthread_cond_signal(&f->cond);
    pthread_mutex_unlock(&f->mutex);

    // Free the packed args (allocated by codegen via malloc)
    if (f->args) {
        free(f->args);
        f->args = NULL;
    }

    return NULL;
}

// chris_async_spawn(func_ptr, arg_ptr, kind) -> Future*
void* chris_async_spawn(void* func_ptr, void* arg_ptr, int kind) {
    chris_future* f = (chris_future*)malloc(sizeof(chris_future));
    if (!f) {
        fprintf(stderr, "Error: failed to allocate async task\n");
        exit(1);
    }

    f->func   = (chris_thunk_fn)func_ptr;
    f->args   = arg_ptr;
    f->kind   = kind;
    f->state  = CHRIS_TASK_PENDING;
    f->result = 0;
    pthread_mutex_init(&f->mutex, NULL);
    pthread_cond_init(&f->cond, NULL);

    chris_register_task(f);

    // Launch the task on a new thread
    int rc = pthread_create(&f->thread, NULL, chris_async_thread_entry, f);
    if (rc != 0) {
        fprintf(stderr, "Error: failed to create async thread (rc=%d)\n", rc);
        exit(1);
    }
    // Detach so resources are cleaned up automatically after join/await
    // (we join explicitly in await, but detach as safety net)

    return (void*)f;
}

// chris_async_await(Future*) -> i64
long long chris_async_await(void* future_ptr) {
    chris_future* f = (chris_future*)future_ptr;
    if (!f) return 0;

    // Wait for the task to complete
    pthread_mutex_lock(&f->mutex);
    while (f->state != CHRIS_TASK_COMPLETED) {
        pthread_cond_wait(&f->cond, &f->mutex);
    }
    long long result = f->result;
    pthread_mutex_unlock(&f->mutex);

    // Join the thread to clean up
    pthread_join(f->thread, NULL);

    // Clean up the future
    pthread_mutex_destroy(&f->mutex);
    pthread_cond_destroy(&f->cond);
    free(f);

    return result;
}

// chris_async_run_loop() -> void
// Drains all pending tasks. Called at end of main.
void chris_async_run_loop(void) {
    pthread_mutex_lock(&chris_registry_mutex);
    int count = chris_task_count;
    pthread_mutex_unlock(&chris_registry_mutex);

    for (int i = 0; i < count; i++) {
        chris_future* f = chris_task_registry[i];
        if (!f) continue;

        pthread_mutex_lock(&f->mutex);
        int state = f->state;
        pthread_mutex_unlock(&f->mutex);

        if (state != CHRIS_TASK_COMPLETED) {
            // Wait for it
            pthread_mutex_lock(&f->mutex);
            while (f->state != CHRIS_TASK_COMPLETED) {
                pthread_cond_wait(&f->cond, &f->mutex);
            }
            pthread_mutex_unlock(&f->mutex);
            pthread_join(f->thread, NULL);
        }
    }

    // Reset registry
    pthread_mutex_lock(&chris_registry_mutex);
    chris_task_count = 0;
    pthread_mutex_unlock(&chris_registry_mutex);
}
