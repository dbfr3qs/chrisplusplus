#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include "gc.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

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
    char* result = (char*)chris_gc_alloc(len_a + len_b + 1, GC_STRING);
    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b);
    result[len_a + len_b] = '\0';
    return result;
}

const char* chris_char_to_str(char c) {
    char* buf = (char*)chris_gc_alloc(2, GC_STRING);
    buf[0] = c;
    buf[1] = '\0';
    return buf;
}

const char* chris_int_to_str(long long val) {
    char* buf = (char*)chris_gc_alloc(32, GC_STRING);
    snprintf(buf, 32, "%lld", val);
    return buf;
}

const char* chris_float_to_str(double val) {
    char* buf = (char*)chris_gc_alloc(64, GC_STRING);
    snprintf(buf, 64, "%g", val);
    return buf;
}

const char* chris_bool_to_str(int val) {
    return val ? "true" : "false";
}

// Array support
void* chris_array_alloc(long long elem_size, long long count) {
    return chris_gc_alloc((size_t)(elem_size * count), GC_ARRAY);
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
    char* result = (char*)chris_gc_alloc(sublen + 1, GC_STRING);
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
    char* result = (char*)chris_gc_alloc(result_len + 1, GC_STRING);
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
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}

const char* chris_str_to_upper(const char* str) {
    if (!str) return "";
    size_t len = strlen(str);
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    for (size_t i = 0; i < len; i++) result[i] = (char)toupper((unsigned char)str[i]);
    result[len] = '\0';
    return result;
}

const char* chris_str_to_lower(const char* str) {
    if (!str) return "";
    size_t len = strlen(str);
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    for (size_t i = 0; i < len; i++) result[i] = (char)tolower((unsigned char)str[i]);
    result[len] = '\0';
    return result;
}

const char* chris_str_char_at(const char* str, long long index) {
    if (!str || index < 0 || (size_t)index >= strlen(str)) return "";
    char* result = (char*)chris_gc_alloc(2, GC_STRING);
    result[0] = str[index];
    result[1] = '\0';
    return result;
}

// Array struct: {i64 length, ptr data}
typedef struct { long long length; void* data; } ChrisArray;

// Array methods
void chris_array_push(ChrisArray* arr, long long elem_size, long long value) {
    long long new_len = arr->length + 1;
    void* new_data = chris_gc_alloc((size_t)(elem_size * new_len), GC_ARRAY);
    if (arr->data && arr->length > 0) {
        memcpy(new_data, arr->data, (size_t)(elem_size * arr->length));
    }
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
        char* empty = (char*)chris_gc_alloc(1, GC_STRING);
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
    char* result = (char*)chris_gc_alloc(total + 1, GC_STRING);
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
    out->data = chris_gc_alloc((size_t)(elem_size * arr->length), GC_ARRAY);
    long long* src = (long long*)arr->data;
    long long* dst = (long long*)out->data;
    for (long long i = 0; i < arr->length; i++) {
        dst[i] = cb(src[i]);
    }
}

void chris_array_filter(ChrisArray* arr, long long elem_size, FilterCallback cb, ChrisArray* out) {
    // Allocate max possible size
    long long* src = (long long*)arr->data;
    long long* tmp = (long long*)chris_gc_alloc((size_t)(elem_size * arr->length), GC_ARRAY);
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
        const char** parts = (const char**)chris_gc_alloc(sizeof(const char*) * slen, GC_ARRAY);
        for (size_t i = 0; i < slen; i++) {
            char* ch = (char*)chris_gc_alloc(2, GC_STRING);
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

    const char** parts = (const char**)chris_gc_alloc(sizeof(const char*) * count, GC_ARRAY);
    p = str;
    for (int i = 0; i < count; i++) {
        const char* next = strstr(p, delim);
        size_t partlen = next ? (size_t)(next - p) : strlen(p);
        char* part = (char*)chris_gc_alloc(partlen + 1, GC_STRING);
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
// Map Runtime Support (string-keyed hash map)
// ============================================================================

#define CHRIS_MAP_INITIAL_CAPACITY 16
#define CHRIS_MAP_LOAD_FACTOR 0.75

typedef struct chris_map_entry {
    const char* key;
    long long value;
    struct chris_map_entry* next;
} chris_map_entry;

typedef struct chris_map {
    chris_map_entry** buckets;
    int capacity;
    int size;
} chris_map;

static unsigned long chris_map_hash(const char* key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++))
        hash = ((hash << 5) + hash) + c; // djb2
    return hash;
}

chris_map* chris_map_create(void) {
    chris_map* m = (chris_map*)malloc(sizeof(chris_map));
    m->capacity = CHRIS_MAP_INITIAL_CAPACITY;
    m->size = 0;
    m->buckets = (chris_map_entry**)calloc(m->capacity, sizeof(chris_map_entry*));
    return m;
}

static void chris_map_resize(chris_map* m) {
    int oldCap = m->capacity;
    chris_map_entry** oldBuckets = m->buckets;
    m->capacity *= 2;
    m->buckets = (chris_map_entry**)calloc(m->capacity, sizeof(chris_map_entry*));
    for (int i = 0; i < oldCap; i++) {
        chris_map_entry* e = oldBuckets[i];
        while (e) {
            chris_map_entry* next = e->next;
            unsigned long idx = chris_map_hash(e->key) % m->capacity;
            e->next = m->buckets[idx];
            m->buckets[idx] = e;
            e = next;
        }
    }
    free(oldBuckets);
}

void chris_map_set(chris_map* m, const char* key, long long value) {
    if (!m || !key) return;
    unsigned long idx = chris_map_hash(key) % m->capacity;
    chris_map_entry* e = m->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;
            return;
        }
        e = e->next;
    }
    // New entry
    chris_map_entry* newEntry = (chris_map_entry*)malloc(sizeof(chris_map_entry));
    size_t klen = strlen(key);
    char* kcopy = (char*)malloc(klen + 1);
    memcpy(kcopy, key, klen + 1);
    newEntry->key = kcopy;
    newEntry->value = value;
    newEntry->next = m->buckets[idx];
    m->buckets[idx] = newEntry;
    m->size++;
    if ((double)m->size / m->capacity > CHRIS_MAP_LOAD_FACTOR) {
        chris_map_resize(m);
    }
}

// Returns the value for the key, or 0 if not found
long long chris_map_get(chris_map* m, const char* key) {
    if (!m || !key) return 0;
    unsigned long idx = chris_map_hash(key) % m->capacity;
    chris_map_entry* e = m->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0) return e->value;
        e = e->next;
    }
    return 0;
}

// Returns 1 if key exists, 0 otherwise
int chris_map_has(chris_map* m, const char* key) {
    if (!m || !key) return 0;
    unsigned long idx = chris_map_hash(key) % m->capacity;
    chris_map_entry* e = m->buckets[idx];
    while (e) {
        if (strcmp(e->key, key) == 0) return 1;
        e = e->next;
    }
    return 0;
}

// Remove a key, returns 1 if found and removed, 0 otherwise
int chris_map_delete(chris_map* m, const char* key) {
    if (!m || !key) return 0;
    unsigned long idx = chris_map_hash(key) % m->capacity;
    chris_map_entry** pp = &m->buckets[idx];
    while (*pp) {
        chris_map_entry* e = *pp;
        if (strcmp(e->key, key) == 0) {
            *pp = e->next;
            free((void*)e->key);
            free(e);
            m->size--;
            return 1;
        }
        pp = &e->next;
    }
    return 0;
}

long long chris_map_size(chris_map* m) {
    if (!m) return 0;
    return (long long)m->size;
}

void chris_map_destroy(chris_map* m) {
    if (!m) return;
    for (int i = 0; i < m->capacity; i++) {
        chris_map_entry* e = m->buckets[i];
        while (e) {
            chris_map_entry* next = e->next;
            free((void*)e->key);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
}

// Get all keys as an array of strings
void chris_map_keys(chris_map* m, ChrisArray* out) {
    if (!m || !out) {
        if (out) { out->length = 0; out->data = NULL; }
        return;
    }
    out->length = m->size;
    out->data = chris_gc_alloc(sizeof(long long) * m->size, GC_ARRAY);
    long long* keys = (long long*)out->data;
    int idx = 0;
    for (int i = 0; i < m->capacity; i++) {
        chris_map_entry* e = m->buckets[i];
        while (e) {
            keys[idx++] = (long long)e->key;
            e = e->next;
        }
    }
}

// ============================================================================
// Set Runtime Support (hash set, string elements)
// ============================================================================

#define CHRIS_SET_INIT_CAP 16
#define CHRIS_SET_LOAD_FACTOR 0.75

typedef struct chris_set_entry {
    const char* value;
    struct chris_set_entry* next;
} chris_set_entry;

typedef struct {
    chris_set_entry** buckets;
    int capacity;
    int size;
} chris_set;

chris_set* chris_set_create(void) {
    chris_set* s = (chris_set*)malloc(sizeof(chris_set));
    s->capacity = CHRIS_SET_INIT_CAP;
    s->size = 0;
    s->buckets = (chris_set_entry**)calloc(s->capacity, sizeof(chris_set_entry*));
    return s;
}

static void chris_set_resize(chris_set* s) {
    int oldCap = s->capacity;
    chris_set_entry** oldBuckets = s->buckets;
    s->capacity *= 2;
    s->buckets = (chris_set_entry**)calloc(s->capacity, sizeof(chris_set_entry*));
    for (int i = 0; i < oldCap; i++) {
        chris_set_entry* e = oldBuckets[i];
        while (e) {
            chris_set_entry* next = e->next;
            unsigned long idx = chris_map_hash(e->value) % s->capacity;
            e->next = s->buckets[idx];
            s->buckets[idx] = e;
            e = next;
        }
    }
    free(oldBuckets);
}

void chris_set_add(chris_set* s, const char* value) {
    if (!s || !value) return;
    unsigned long idx = chris_map_hash(value) % s->capacity;
    chris_set_entry* e = s->buckets[idx];
    while (e) {
        if (strcmp(e->value, value) == 0) return; // already exists
        e = e->next;
    }
    chris_set_entry* newEntry = (chris_set_entry*)malloc(sizeof(chris_set_entry));
    size_t len = strlen(value);
    char* vcopy = (char*)malloc(len + 1);
    memcpy(vcopy, value, len + 1);
    newEntry->value = vcopy;
    newEntry->next = s->buckets[idx];
    s->buckets[idx] = newEntry;
    s->size++;
    if ((double)s->size / s->capacity > CHRIS_SET_LOAD_FACTOR) {
        chris_set_resize(s);
    }
}

int chris_set_has(chris_set* s, const char* value) {
    if (!s || !value) return 0;
    unsigned long idx = chris_map_hash(value) % s->capacity;
    chris_set_entry* e = s->buckets[idx];
    while (e) {
        if (strcmp(e->value, value) == 0) return 1;
        e = e->next;
    }
    return 0;
}

int chris_set_remove(chris_set* s, const char* value) {
    if (!s || !value) return 0;
    unsigned long idx = chris_map_hash(value) % s->capacity;
    chris_set_entry** prev = &s->buckets[idx];
    chris_set_entry* e = s->buckets[idx];
    while (e) {
        if (strcmp(e->value, value) == 0) {
            *prev = e->next;
            free((void*)e->value);
            free(e);
            s->size--;
            return 1;
        }
        prev = &e->next;
        e = e->next;
    }
    return 0;
}

long long chris_set_size(chris_set* s) {
    if (!s) return 0;
    return s->size;
}

void chris_set_clear(chris_set* s) {
    if (!s) return;
    for (int i = 0; i < s->capacity; i++) {
        chris_set_entry* e = s->buckets[i];
        while (e) {
            chris_set_entry* next = e->next;
            free((void*)e->value);
            free(e);
            e = next;
        }
        s->buckets[i] = NULL;
    }
    s->size = 0;
}

void chris_set_values(chris_set* s, ChrisArray* out) {
    if (!s || !out) {
        if (out) { out->length = 0; out->data = NULL; }
        return;
    }
    out->length = s->size;
    out->data = chris_gc_alloc(sizeof(long long) * s->size, GC_ARRAY);
    long long* vals = (long long*)out->data;
    int idx = 0;
    for (int i = 0; i < s->capacity; i++) {
        chris_set_entry* e = s->buckets[i];
        while (e) {
            vals[idx++] = (long long)e->value;
            e = e->next;
        }
    }
}

void chris_set_destroy(chris_set* s) {
    if (!s) return;
    for (int i = 0; i < s->capacity; i++) {
        chris_set_entry* e = s->buckets[i];
        while (e) {
            chris_set_entry* next = e->next;
            free((void*)e->value);
            free(e);
            e = next;
        }
    }
    free(s->buckets);
    free(s);
}

// ============================================================================
// Reflection Runtime Support
// ============================================================================

typedef struct {
    const char* name;       // class name e.g. "User"
    long long num_fields;
    const char** fields;    // array of field name strings
    long long num_implements;
    const char** implements; // array of interface name strings
} ChrisTypeInfo;

// Get type name from TypeInfo
const char* chris_typeinfo_name(ChrisTypeInfo* info) {
    if (!info) return "<unknown>";
    return info->name;
}

// Get fields as a ChrisArray of strings
void chris_typeinfo_fields(ChrisTypeInfo* info, ChrisArray* out) {
    if (!info) { out->length = 0; out->data = NULL; return; }
    out->length = info->num_fields;
    out->data = chris_gc_alloc(sizeof(long long) * info->num_fields, GC_ARRAY);
    long long* arr = (long long*)out->data;
    for (long long i = 0; i < info->num_fields; i++) {
        arr[i] = (long long)info->fields[i];
    }
}

// Get implements as a ChrisArray of strings
void chris_typeinfo_implements(ChrisTypeInfo* info, ChrisArray* out) {
    if (!info) { out->length = 0; out->data = NULL; return; }
    out->length = info->num_implements;
    out->data = chris_gc_alloc(sizeof(long long) * info->num_implements, GC_ARRAY);
    long long* arr = (long long*)out->data;
    for (long long i = 0; i < info->num_implements; i++) {
        arr[i] = (long long)info->implements[i];
    }
}

// ============================================================================
// Process Execution Runtime Support
// ============================================================================

// Execute a shell command and return exit code
long long chris_exec(const char* command) {
    if (!command) return -1;
    int status = system(command);
#ifdef _WIN32
    return (long long)status;
#else
    if (WIFEXITED(status)) return (long long)WEXITSTATUS(status);
    return (long long)status;
#endif
}

// Execute a shell command and capture stdout as a string
const char* chris_exec_output(const char* command) {
    if (!command) return "";
    FILE* fp = popen(command, "r");
    if (!fp) return "";
    size_t capacity = 1024;
    size_t length = 0;
    char* buf = (char*)malloc(capacity);
    if (!buf) { pclose(fp); return ""; }
    char tmp[256];
    while (fgets(tmp, sizeof(tmp), fp)) {
        size_t n = strlen(tmp);
        if (length + n + 1 > capacity) {
            capacity *= 2;
            char* newbuf = (char*)realloc(buf, capacity);
            if (!newbuf) { free(buf); pclose(fp); return ""; }
            buf = newbuf;
        }
        memcpy(buf + length, tmp, n);
        length += n;
    }
    pclose(fp);
    // Copy into GC-managed string
    char* result = (char*)chris_gc_alloc(length + 1, GC_STRING);
    memcpy(result, buf, length);
    result[length] = '\0';
    free(buf);
    return result;
}

// ============================================================================
// File I/O Runtime Support
// ============================================================================

// Read entire file contents as a string. Returns "" on error.
const char* chris_read_file(const char* path) {
    if (!path) return "";
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return ""; }
    char* buf = (char*)chris_gc_alloc((size_t)size + 1, GC_STRING);
    size_t read = fread(buf, 1, (size_t)size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

// Write string to file. Returns 1 on success, 0 on failure.
int chris_write_file(const char* path, const char* content) {
    if (!path || !content) return 0;
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 1 : 0;
}

// Append string to file. Returns 1 on success, 0 on failure.
int chris_append_file(const char* path, const char* content) {
    if (!path || !content) return 0;
    FILE* f = fopen(path, "ab");
    if (!f) return 0;
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return (written == len) ? 1 : 0;
}

// Check if file exists. Returns 1 if exists, 0 otherwise.
int chris_file_exists(const char* path) {
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
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

    // Args are GC-managed, no need to free manually
    f->args = NULL;

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

// ============================================================================
// Networking Runtime Support (TCP, UDP, DNS)
// ============================================================================

// TCP: connect to host:port, returns socket fd or -1 on error
long long chris_tcp_connect(const char* host, long long port) {
    if (!host) return -1;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%lld", port);

    if (getaddrinfo(host, portStr, &hints, &res) != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    return (long long)fd;
}

// TCP: create a listening server socket on port, returns fd or -1
long long chris_tcp_listen(long long port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 128) < 0) {
        close(fd);
        return -1;
    }
    return (long long)fd;
}

// TCP: accept a connection on a listening socket, returns client fd or -1
long long chris_tcp_accept(long long serverFd) {
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept((int)serverFd, (struct sockaddr*)&clientAddr, &len);
    return (long long)clientFd;
}

// TCP: send data on socket, returns bytes sent or -1
long long chris_tcp_send(long long fd, const char* data) {
    if (!data) return -1;
    ssize_t sent = send((int)fd, data, strlen(data), 0);
    return (long long)sent;
}

// TCP: receive up to maxBytes from socket, returns heap-allocated string
const char* chris_tcp_recv(long long fd, long long maxBytes) {
    if (maxBytes <= 0) maxBytes = 4096;
    char* buf = (char*)chris_gc_alloc(maxBytes + 1, GC_STRING);
    ssize_t n = recv((int)fd, buf, maxBytes, 0);
    if (n <= 0) {
        buf[0] = '\0';
        return buf;
    }
    buf[n] = '\0';
    return buf;
}

// TCP: close a socket
void chris_tcp_close(long long fd) {
    close((int)fd);
}

// UDP: create a UDP socket, returns fd or -1
long long chris_udp_create(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    return (long long)fd;
}

// UDP: bind socket to port, returns 0 on success, -1 on error
long long chris_udp_bind(long long fd, long long port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);
    if (bind((int)fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    return 0;
}

// UDP: send data to host:port, returns bytes sent or -1
long long chris_udp_send_to(long long fd, const char* host, long long port, const char* data) {
    if (!host || !data) return -1;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%lld", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0) return -1;

    ssize_t sent = sendto((int)fd, data, strlen(data), 0, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    return (long long)sent;
}

// UDP: receive data, returns heap-allocated string
const char* chris_udp_recv_from(long long fd, long long maxBytes) {
    if (maxBytes <= 0) maxBytes = 4096;
    char* buf = (char*)chris_gc_alloc(maxBytes + 1, GC_STRING);
    struct sockaddr_in srcAddr;
    socklen_t srcLen = sizeof(srcAddr);
    ssize_t n = recvfrom((int)fd, buf, maxBytes, 0, (struct sockaddr*)&srcAddr, &srcLen);
    if (n <= 0) {
        buf[0] = '\0';
        return buf;
    }
    buf[n] = '\0';
    return buf;
}

// UDP: close socket
void chris_udp_close(long long fd) {
    close((int)fd);
}

// DNS: resolve hostname to first IPv4 address string
const char* chris_dns_lookup(const char* hostname) {
    if (!hostname) return "";
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) return "";

    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    const char* ip = inet_ntoa(addr->sin_addr);
    size_t len = strlen(ip);
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    memcpy(result, ip, len + 1);
    freeaddrinfo(res);
    return result;
}

// ============================================================================
// HTTP Runtime Support (Client + Server)
// ============================================================================

// Internal helper: parse URL into host, port, path
// Supports http://host:port/path and http://host/path (default port 80)
static int chris_http_parse_url(const char* url, char* host, int hostLen,
                                 int* port, char* path, int pathLen) {
    if (!url) return -1;
    const char* p = url;
    // Skip scheme
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) p += 8;

    // Extract host
    const char* hostStart = p;
    const char* colonPos = NULL;
    const char* slashPos = NULL;
    while (*p && *p != '/' && *p != ':') p++;
    if (*p == ':') { colonPos = p; while (*p && *p != '/') p++; }
    if (*p == '/') slashPos = p;

    // Host
    int hLen = (int)((colonPos ? colonPos : (slashPos ? slashPos : p)) - hostStart);
    if (hLen >= hostLen) hLen = hostLen - 1;
    memcpy(host, hostStart, hLen);
    host[hLen] = '\0';

    // Port
    if (colonPos) {
        *port = atoi(colonPos + 1);
    } else {
        *port = 80;
    }

    // Path
    if (slashPos) {
        int pLen = (int)strlen(slashPos);
        if (pLen >= pathLen) pLen = pathLen - 1;
        memcpy(path, slashPos, pLen);
        path[pLen] = '\0';
    } else {
        path[0] = '/';
        path[1] = '\0';
    }
    return 0;
}

// Internal helper: read full HTTP response body from socket
static const char* chris_http_read_response(int fd) {
    // Read headers + body into temp buffer (uses malloc+realloc for dynamic growth)
    size_t bufSize = 8192;
    size_t totalRead = 0;
    char* buf = (char*)malloc(bufSize);

    while (1) {
        if (totalRead >= bufSize - 1) {
            bufSize *= 2;
            buf = (char*)realloc(buf, bufSize);
        }
        ssize_t n = recv(fd, buf + totalRead, bufSize - totalRead - 1, 0);
        if (n <= 0) break;
        totalRead += n;
    }
    buf[totalRead] = '\0';

    // Find body after \r\n\r\n
    char* body = strstr(buf, "\r\n\r\n");
    const char* src = body ? body + 4 : buf;
    size_t srcLen = strlen(src);
    char* result = (char*)chris_gc_alloc(srcLen + 1, GC_STRING);
    memcpy(result, src, srcLen + 1);
    free(buf);
    return result;
}

// HTTP GET: returns response body as heap-allocated string
const char* chris_http_get(const char* url) {
    if (!url) return "";
    char host[256], path[2048];
    int port;
    if (chris_http_parse_url(url, host, 256, &port, path, 2048) < 0) return "";

    long long fd = chris_tcp_connect(host, port);
    if (fd < 0) return "";

    // Build request
    char request[4096];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);

    send((int)fd, request, strlen(request), 0);
    const char* result = chris_http_read_response((int)fd);
    close((int)fd);
    return result;
}

// HTTP POST: returns response body as heap-allocated string
const char* chris_http_post(const char* url, const char* body, const char* contentType) {
    if (!url) return "";
    if (!body) body = "";
    if (!contentType) contentType = "text/plain";
    char host[256], path[2048];
    int port;
    if (chris_http_parse_url(url, host, 256, &port, path, 2048) < 0) return "";

    long long fd = chris_tcp_connect(host, port);
    if (fd < 0) return "";

    // Build request
    char request[8192];
    snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: %s\r\n"
        "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
        path, host, contentType, strlen(body), body);

    send((int)fd, request, strlen(request), 0);
    const char* result = chris_http_read_response((int)fd);
    close((int)fd);
    return result;
}

// HTTP Server request struct
typedef struct {
    int clientFd;
    char method[16];
    char path[2048];
    char* body;
    size_t bodyLen;
} chris_http_request;

// HTTP Server: create a listening HTTP server on port
long long chris_http_server_create(long long port) {
    return chris_tcp_listen(port);
}

// HTTP Server: accept next HTTP request, returns handle (pointer as i64)
long long chris_http_server_accept(long long serverFd) {
    long long clientFd = chris_tcp_accept(serverFd);
    if (clientFd < 0) return 0;

    // Read the HTTP request
    char buf[8192];
    ssize_t n = recv((int)clientFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close((int)clientFd);
        return 0;
    }
    buf[n] = '\0';

    chris_http_request* req = (chris_http_request*)calloc(1, sizeof(chris_http_request));
    req->clientFd = (int)clientFd;

    // Parse method and path from first line: "METHOD /path HTTP/1.x"
    char* sp1 = strchr(buf, ' ');
    if (sp1) {
        int mLen = (int)(sp1 - buf);
        if (mLen > 15) mLen = 15;
        memcpy(req->method, buf, mLen);
        req->method[mLen] = '\0';

        char* pathStart = sp1 + 1;
        char* sp2 = strchr(pathStart, ' ');
        if (sp2) {
            int pLen = (int)(sp2 - pathStart);
            if (pLen > 2047) pLen = 2047;
            memcpy(req->path, pathStart, pLen);
            req->path[pLen] = '\0';
        }
    }

    // Extract body after \r\n\r\n
    char* bodyStart = strstr(buf, "\r\n\r\n");
    if (bodyStart) {
        bodyStart += 4;
        size_t bLen = strlen(bodyStart);
        req->body = (char*)chris_gc_alloc(bLen + 1, GC_STRING);
        memcpy(req->body, bodyStart, bLen + 1);
        req->bodyLen = bLen;
    } else {
        req->body = (char*)chris_gc_alloc(1, GC_STRING);
        req->bodyLen = 0;
    }

    return (long long)(uintptr_t)req;
}

// HTTP Server: get request method
const char* chris_http_request_method(long long handle) {
    if (!handle) return "";
    chris_http_request* req = (chris_http_request*)(uintptr_t)handle;
    size_t len = strlen(req->method);
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    memcpy(result, req->method, len + 1);
    return result;
}

// HTTP Server: get request path
const char* chris_http_request_path(long long handle) {
    if (!handle) return "";
    chris_http_request* req = (chris_http_request*)(uintptr_t)handle;
    size_t len = strlen(req->path);
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    memcpy(result, req->path, len + 1);
    return result;
}

// HTTP Server: get request body
const char* chris_http_request_body(long long handle) {
    if (!handle) return "";
    chris_http_request* req = (chris_http_request*)(uintptr_t)handle;
    size_t len = req->bodyLen;
    char* result = (char*)chris_gc_alloc(len + 1, GC_STRING);
    memcpy(result, req->body, len + 1);
    return result;
}

// HTTP Server: send response and close connection
void chris_http_respond(long long handle, long long statusCode, const char* body) {
    if (!handle) return;
    chris_http_request* req = (chris_http_request*)(uintptr_t)handle;
    if (!body) body = "";

    const char* statusText = "OK";
    if (statusCode == 201) statusText = "Created";
    else if (statusCode == 204) statusText = "No Content";
    else if (statusCode == 400) statusText = "Bad Request";
    else if (statusCode == 404) statusText = "Not Found";
    else if (statusCode == 500) statusText = "Internal Server Error";

    char header[512];
    snprintf(header, sizeof(header),
        "HTTP/1.1 %lld %s\r\nContent-Length: %zu\r\n"
        "Content-Type: text/plain\r\nConnection: close\r\n\r\n",
        statusCode, statusText, strlen(body));

    send(req->clientFd, header, strlen(header), 0);
    if (strlen(body) > 0) {
        send(req->clientFd, body, strlen(body), 0);
    }
    close(req->clientFd);
    if (req->body) free(req->body);
    free(req);
}

// HTTP Server: close server socket
void chris_http_server_close(long long serverFd) {
    close((int)serverFd);
}

// ============================================================================
// Concurrent Runtime Support (ConcurrentMap, ConcurrentQueue, Atomics)
// ============================================================================

// --- ConcurrentMap: thread-safe wrapper around chris_map ---

typedef struct {
    chris_map*      map;
    pthread_mutex_t mutex;
} chris_concurrent_map;

void* chris_cmap_create(void) {
    chris_concurrent_map* cm = (chris_concurrent_map*)malloc(sizeof(chris_concurrent_map));
    cm->map = (chris_map*)chris_map_create();
    pthread_mutex_init(&cm->mutex, NULL);
    return cm;
}

void chris_cmap_set(void* handle, const char* key, long long value) {
    chris_concurrent_map* cm = (chris_concurrent_map*)handle;
    pthread_mutex_lock(&cm->mutex);
    chris_map_set(cm->map, key, value);
    pthread_mutex_unlock(&cm->mutex);
}

long long chris_cmap_get(void* handle, const char* key) {
    chris_concurrent_map* cm = (chris_concurrent_map*)handle;
    pthread_mutex_lock(&cm->mutex);
    long long val = chris_map_get(cm->map, key);
    pthread_mutex_unlock(&cm->mutex);
    return val;
}

long long chris_cmap_has(void* handle, const char* key) {
    chris_concurrent_map* cm = (chris_concurrent_map*)handle;
    pthread_mutex_lock(&cm->mutex);
    long long result = chris_map_has(cm->map, key);
    pthread_mutex_unlock(&cm->mutex);
    return result;
}

long long chris_cmap_delete(void* handle, const char* key) {
    chris_concurrent_map* cm = (chris_concurrent_map*)handle;
    pthread_mutex_lock(&cm->mutex);
    long long result = chris_map_delete(cm->map, key);
    pthread_mutex_unlock(&cm->mutex);
    return result;
}

long long chris_cmap_size(void* handle) {
    chris_concurrent_map* cm = (chris_concurrent_map*)handle;
    pthread_mutex_lock(&cm->mutex);
    long long sz = chris_map_size(cm->map);
    pthread_mutex_unlock(&cm->mutex);
    return sz;
}

void chris_cmap_destroy(void* handle) {
    chris_concurrent_map* cm = (chris_concurrent_map*)handle;
    pthread_mutex_destroy(&cm->mutex);
    chris_map_destroy(cm->map);
    free(cm);
}

// --- ConcurrentQueue: thread-safe bounded FIFO queue ---

#define CHRIS_CQUEUE_DEFAULT_CAP 1024

typedef struct {
    long long*      buffer;
    int             capacity;
    int             head;
    int             tail;
    int             count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} chris_concurrent_queue;

void* chris_cqueue_create(void) {
    chris_concurrent_queue* q = (chris_concurrent_queue*)malloc(sizeof(chris_concurrent_queue));
    q->capacity = CHRIS_CQUEUE_DEFAULT_CAP;
    q->buffer = (long long*)malloc(sizeof(long long) * q->capacity);
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return q;
}

void chris_cqueue_enqueue(void* handle, long long value) {
    chris_concurrent_queue* q = (chris_concurrent_queue*)handle;
    pthread_mutex_lock(&q->mutex);
    while (q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }
    q->buffer[q->tail] = value;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

long long chris_cqueue_dequeue(void* handle) {
    chris_concurrent_queue* q = (chris_concurrent_queue*)handle;
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    long long value = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return value;
}

long long chris_cqueue_size(void* handle) {
    chris_concurrent_queue* q = (chris_concurrent_queue*)handle;
    pthread_mutex_lock(&q->mutex);
    long long sz = q->count;
    pthread_mutex_unlock(&q->mutex);
    return sz;
}

long long chris_cqueue_is_empty(void* handle) {
    chris_concurrent_queue* q = (chris_concurrent_queue*)handle;
    pthread_mutex_lock(&q->mutex);
    long long empty = (q->count == 0) ? 1 : 0;
    pthread_mutex_unlock(&q->mutex);
    return empty;
}

void chris_cqueue_destroy(void* handle) {
    chris_concurrent_queue* q = (chris_concurrent_queue*)handle;
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q->buffer);
    free(q);
}

// --- Atomics: atomic integer operations ---

typedef struct {
    long long value;
    pthread_mutex_t mutex;
} chris_atomic;

long long chris_atomic_create(long long initial) {
    chris_atomic* a = (chris_atomic*)malloc(sizeof(chris_atomic));
    a->value = initial;
    pthread_mutex_init(&a->mutex, NULL);
    return (long long)(uintptr_t)a;
}

long long chris_atomic_load(long long handle) {
    chris_atomic* a = (chris_atomic*)(uintptr_t)handle;
    pthread_mutex_lock(&a->mutex);
    long long val = a->value;
    pthread_mutex_unlock(&a->mutex);
    return val;
}

void chris_atomic_store(long long handle, long long value) {
    chris_atomic* a = (chris_atomic*)(uintptr_t)handle;
    pthread_mutex_lock(&a->mutex);
    a->value = value;
    pthread_mutex_unlock(&a->mutex);
}

long long chris_atomic_add(long long handle, long long delta) {
    chris_atomic* a = (chris_atomic*)(uintptr_t)handle;
    pthread_mutex_lock(&a->mutex);
    a->value += delta;
    long long result = a->value;
    pthread_mutex_unlock(&a->mutex);
    return result;
}

long long chris_atomic_sub(long long handle, long long delta) {
    chris_atomic* a = (chris_atomic*)(uintptr_t)handle;
    pthread_mutex_lock(&a->mutex);
    a->value -= delta;
    long long result = a->value;
    pthread_mutex_unlock(&a->mutex);
    return result;
}

long long chris_atomic_compare_swap(long long handle, long long expected, long long desired) {
    chris_atomic* a = (chris_atomic*)(uintptr_t)handle;
    pthread_mutex_lock(&a->mutex);
    long long success = 0;
    if (a->value == expected) {
        a->value = desired;
        success = 1;
    }
    pthread_mutex_unlock(&a->mutex);
    return success;
}

void chris_atomic_destroy(long long handle) {
    chris_atomic* a = (chris_atomic*)(uintptr_t)handle;
    pthread_mutex_destroy(&a->mutex);
    free(a);
}

// ============================================================================
// Math Runtime Support
// ============================================================================

static int chris_random_seeded = 0;

long long chris_math_abs(long long x) {
    return x < 0 ? -x : x;
}

long long chris_math_min(long long a, long long b) {
    return a < b ? a : b;
}

long long chris_math_max(long long a, long long b) {
    return a > b ? a : b;
}

double chris_math_sqrt(double x) {
    return sqrt(x);
}

double chris_math_pow(double base, double exp) {
    return pow(base, exp);
}

double chris_math_floor(double x) {
    return floor(x);
}

double chris_math_ceil(double x) {
    return ceil(x);
}

double chris_math_round(double x) {
    return round(x);
}

double chris_math_log(double x) {
    return log(x);
}

double chris_math_sin(double x) {
    return sin(x);
}

double chris_math_cos(double x) {
    return cos(x);
}

double chris_math_tan(double x) {
    return tan(x);
}

double chris_math_fabs(double x) {
    return fabs(x);
}

double chris_math_fmin(double a, double b) {
    return fmin(a, b);
}

double chris_math_fmax(double a, double b) {
    return fmax(a, b);
}

long long chris_math_random(long long lo, long long hi) {
    if (!chris_random_seeded) {
        srand((unsigned int)time(NULL));
        chris_random_seeded = 1;
    }
    if (lo >= hi) return lo;
    return lo + (rand() % (hi - lo + 1));
}

// ============================================================================
// Stdin Runtime Support
// ============================================================================

const char* chris_read_line(void) {
    char buf[4096];
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        char* empty = (char*)chris_gc_alloc(1, GC_STRING);
        empty[0] = '\0';
        return empty;
    }
    // Strip trailing newline
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    char* result = (char*)chris_gc_alloc(strlen(buf) + 1, GC_STRING);
    strcpy(result, buf);
    return result;
}

// ============================================================================
// Assertion Runtime Support (for chris test framework)
// ============================================================================

static int chris_assert_total = 0;
static int chris_assert_passed = 0;
static int chris_assert_failed = 0;

void chris_assert(int condition, const char* message, const char* file, long long line) {
    chris_assert_total++;
    if (condition) {
        chris_assert_passed++;
    } else {
        chris_assert_failed++;
        fprintf(stderr, "  FAIL: %s (%s:%lld)\n", message ? message : "assertion failed", file, line);
    }
}

void chris_assert_equal_int(long long expected, long long actual, const char* message, long long line) {
    chris_assert_total++;
    if (expected == actual) {
        chris_assert_passed++;
    } else {
        chris_assert_failed++;
        fprintf(stderr, "  FAIL: %s — expected %lld, got %lld (line %lld)\n",
                message ? message : "assertEqual", expected, actual, line);
    }
}

void chris_assert_equal_str(const char* expected, const char* actual, const char* message, long long line) {
    chris_assert_total++;
    if (expected && actual && strcmp(expected, actual) == 0) {
        chris_assert_passed++;
    } else {
        chris_assert_failed++;
        fprintf(stderr, "  FAIL: %s — expected \"%s\", got \"%s\" (line %lld)\n",
                message ? message : "assertEqual",
                expected ? expected : "(null)",
                actual ? actual : "(null)", line);
    }
}

void chris_assert_summary(void) {
    printf("\n--- Test Results ---\n");
    printf("Total: %d | Passed: %d | Failed: %d\n", chris_assert_total, chris_assert_passed, chris_assert_failed);
    if (chris_assert_failed > 0) {
        printf("FAILED\n");
    } else {
        printf("PASSED\n");
    }
}

int chris_assert_exit_code(void) {
    return chris_assert_failed > 0 ? 1 : 0;
}

// ============================================================================
// JSON Runtime Support
// ============================================================================

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_INT, JSON_FLOAT, JSON_STRING, JSON_ARRAY, JSON_OBJECT
} chris_json_type;

typedef struct chris_json_node chris_json_node;

typedef struct {
    char* key;
    chris_json_node* value;
} chris_json_pair;

struct chris_json_node {
    chris_json_type type;
    union {
        long long int_val;
        double float_val;
        int bool_val;
        char* string_val;
        struct { chris_json_node** items; int count; int cap; } array_val;
        struct { chris_json_pair* pairs; int count; int cap; } object_val;
    };
};

static void json_skip_ws(const char** p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;
}

static chris_json_node* json_parse_value(const char** p);

static chris_json_node* json_new_node(chris_json_type type) {
    chris_json_node* n = (chris_json_node*)calloc(1, sizeof(chris_json_node));
    n->type = type;
    return n;
}

static chris_json_node* json_parse_string(const char** p) {
    if (**p != '"') return NULL;
    (*p)++; // skip opening "
    const char* start = *p;
    // Find end, handling escapes
    char buf[4096];
    int bi = 0;
    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
                case '"': buf[bi++] = '"'; break;
                case '\\': buf[bi++] = '\\'; break;
                case 'n': buf[bi++] = '\n'; break;
                case 't': buf[bi++] = '\t'; break;
                case 'r': buf[bi++] = '\r'; break;
                case '/': buf[bi++] = '/'; break;
                default: buf[bi++] = **p; break;
            }
        } else {
            buf[bi++] = **p;
        }
        (*p)++;
    }
    if (**p == '"') (*p)++;
    buf[bi] = '\0';
    chris_json_node* n = json_new_node(JSON_STRING);
    n->string_val = (char*)malloc(bi + 1);
    memcpy(n->string_val, buf, bi + 1);
    return n;
}

static chris_json_node* json_parse_number(const char** p) {
    const char* start = *p;
    int is_float = 0;
    if (**p == '-') (*p)++;
    while (**p >= '0' && **p <= '9') (*p)++;
    if (**p == '.') { is_float = 1; (*p)++; while (**p >= '0' && **p <= '9') (*p)++; }
    if (**p == 'e' || **p == 'E') { is_float = 1; (*p)++; if (**p == '+' || **p == '-') (*p)++; while (**p >= '0' && **p <= '9') (*p)++; }
    if (is_float) {
        chris_json_node* n = json_new_node(JSON_FLOAT);
        n->float_val = strtod(start, NULL);
        return n;
    } else {
        chris_json_node* n = json_new_node(JSON_INT);
        n->int_val = strtoll(start, NULL, 10);
        return n;
    }
}

static chris_json_node* json_parse_array(const char** p) {
    (*p)++; // skip [
    chris_json_node* n = json_new_node(JSON_ARRAY);
    n->array_val.cap = 8;
    n->array_val.items = (chris_json_node**)malloc(sizeof(chris_json_node*) * n->array_val.cap);
    n->array_val.count = 0;
    json_skip_ws(p);
    if (**p == ']') { (*p)++; return n; }
    while (1) {
        json_skip_ws(p);
        chris_json_node* item = json_parse_value(p);
        if (n->array_val.count >= n->array_val.cap) {
            n->array_val.cap *= 2;
            n->array_val.items = (chris_json_node**)realloc(n->array_val.items, sizeof(chris_json_node*) * n->array_val.cap);
        }
        n->array_val.items[n->array_val.count++] = item;
        json_skip_ws(p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == ']') { (*p)++; break; }
        break; // malformed
    }
    return n;
}

static chris_json_node* json_parse_object(const char** p) {
    (*p)++; // skip {
    chris_json_node* n = json_new_node(JSON_OBJECT);
    n->object_val.cap = 8;
    n->object_val.pairs = (chris_json_pair*)malloc(sizeof(chris_json_pair) * n->object_val.cap);
    n->object_val.count = 0;
    json_skip_ws(p);
    if (**p == '}') { (*p)++; return n; }
    while (1) {
        json_skip_ws(p);
        chris_json_node* key = json_parse_string(p);
        if (!key) break;
        json_skip_ws(p);
        if (**p == ':') (*p)++;
        json_skip_ws(p);
        chris_json_node* val = json_parse_value(p);
        if (n->object_val.count >= n->object_val.cap) {
            n->object_val.cap *= 2;
            n->object_val.pairs = (chris_json_pair*)realloc(n->object_val.pairs, sizeof(chris_json_pair) * n->object_val.cap);
        }
        n->object_val.pairs[n->object_val.count].key = key->string_val;
        n->object_val.pairs[n->object_val.count].value = val;
        n->object_val.count++;
        free(key); // free the wrapper, key string is now owned by pair
        json_skip_ws(p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == '}') { (*p)++; break; }
        break;
    }
    return n;
}

static chris_json_node* json_parse_value(const char** p) {
    json_skip_ws(p);
    if (**p == '"') return json_parse_string(p);
    if (**p == '{') return json_parse_object(p);
    if (**p == '[') return json_parse_array(p);
    if (**p == '-' || (**p >= '0' && **p <= '9')) return json_parse_number(p);
    if (strncmp(*p, "true", 4) == 0) { *p += 4; chris_json_node* n = json_new_node(JSON_BOOL); n->bool_val = 1; return n; }
    if (strncmp(*p, "false", 5) == 0) { *p += 5; chris_json_node* n = json_new_node(JSON_BOOL); n->bool_val = 0; return n; }
    if (strncmp(*p, "null", 4) == 0) { *p += 4; return json_new_node(JSON_NULL); }
    return json_new_node(JSON_NULL);
}

// Public API
long long chris_json_parse(const char* str) {
    if (!str) return 0;
    const char* p = str;
    chris_json_node* root = json_parse_value(&p);
    return (long long)root;
}

const char* chris_json_get(long long handle, const char* key) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_OBJECT) return "";
    for (int i = 0; i < n->object_val.count; i++) {
        if (strcmp(n->object_val.pairs[i].key, key) == 0) {
            chris_json_node* v = n->object_val.pairs[i].value;
            if (v->type == JSON_STRING) return v->string_val;
            return "";
        }
    }
    return "";
}

long long chris_json_get_int(long long handle, const char* key) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_OBJECT) return 0;
    for (int i = 0; i < n->object_val.count; i++) {
        if (strcmp(n->object_val.pairs[i].key, key) == 0) {
            chris_json_node* v = n->object_val.pairs[i].value;
            if (v->type == JSON_INT) return v->int_val;
            if (v->type == JSON_FLOAT) return (long long)v->float_val;
            return 0;
        }
    }
    return 0;
}

long long chris_json_get_bool(long long handle, const char* key) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_OBJECT) return 0;
    for (int i = 0; i < n->object_val.count; i++) {
        if (strcmp(n->object_val.pairs[i].key, key) == 0) {
            chris_json_node* v = n->object_val.pairs[i].value;
            if (v->type == JSON_BOOL) return v->bool_val;
            return 0;
        }
    }
    return 0;
}

double chris_json_get_float(long long handle, const char* key) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_OBJECT) return 0.0;
    for (int i = 0; i < n->object_val.count; i++) {
        if (strcmp(n->object_val.pairs[i].key, key) == 0) {
            chris_json_node* v = n->object_val.pairs[i].value;
            if (v->type == JSON_FLOAT) return v->float_val;
            if (v->type == JSON_INT) return (double)v->int_val;
            return 0.0;
        }
    }
    return 0.0;
}

long long chris_json_get_array(long long handle, const char* key) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_OBJECT) return 0;
    for (int i = 0; i < n->object_val.count; i++) {
        if (strcmp(n->object_val.pairs[i].key, key) == 0) {
            chris_json_node* v = n->object_val.pairs[i].value;
            if (v->type == JSON_ARRAY) return (long long)v;
            return 0;
        }
    }
    return 0;
}

long long chris_json_get_object(long long handle, const char* key) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_OBJECT) return 0;
    for (int i = 0; i < n->object_val.count; i++) {
        if (strcmp(n->object_val.pairs[i].key, key) == 0) {
            chris_json_node* v = n->object_val.pairs[i].value;
            if (v->type == JSON_OBJECT) return (long long)v;
            return 0;
        }
    }
    return 0;
}

long long chris_json_array_length(long long handle) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_ARRAY) return 0;
    return n->array_val.count;
}

long long chris_json_array_get(long long handle, long long index) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n || n->type != JSON_ARRAY || index < 0 || index >= n->array_val.count) return 0;
    return (long long)n->array_val.items[index];
}

// Stringify helpers
static void json_stringify_node(chris_json_node* n, char* buf, int* pos, int cap);

static void json_buf_append(char* buf, int* pos, int cap, const char* str) {
    while (*str && *pos < cap - 1) buf[(*pos)++] = *str++;
}

static void json_stringify_node(chris_json_node* n, char* buf, int* pos, int cap) {
    if (!n) { json_buf_append(buf, pos, cap, "null"); return; }
    switch (n->type) {
        case JSON_NULL: json_buf_append(buf, pos, cap, "null"); break;
        case JSON_BOOL: json_buf_append(buf, pos, cap, n->bool_val ? "true" : "false"); break;
        case JSON_INT: { char tmp[32]; snprintf(tmp, sizeof(tmp), "%lld", n->int_val); json_buf_append(buf, pos, cap, tmp); break; }
        case JSON_FLOAT: { char tmp[64]; snprintf(tmp, sizeof(tmp), "%g", n->float_val); json_buf_append(buf, pos, cap, tmp); break; }
        case JSON_STRING:
            json_buf_append(buf, pos, cap, "\"");
            // Escape special characters
            for (const char* s = n->string_val; *s; s++) {
                if (*s == '"') json_buf_append(buf, pos, cap, "\\\"");
                else if (*s == '\\') json_buf_append(buf, pos, cap, "\\\\");
                else if (*s == '\n') json_buf_append(buf, pos, cap, "\\n");
                else if (*s == '\t') json_buf_append(buf, pos, cap, "\\t");
                else { buf[(*pos)++] = *s; }
            }
            json_buf_append(buf, pos, cap, "\"");
            break;
        case JSON_ARRAY:
            json_buf_append(buf, pos, cap, "[");
            for (int i = 0; i < n->array_val.count; i++) {
                if (i > 0) json_buf_append(buf, pos, cap, ",");
                json_stringify_node(n->array_val.items[i], buf, pos, cap);
            }
            json_buf_append(buf, pos, cap, "]");
            break;
        case JSON_OBJECT:
            json_buf_append(buf, pos, cap, "{");
            for (int i = 0; i < n->object_val.count; i++) {
                if (i > 0) json_buf_append(buf, pos, cap, ",");
                json_buf_append(buf, pos, cap, "\"");
                json_buf_append(buf, pos, cap, n->object_val.pairs[i].key);
                json_buf_append(buf, pos, cap, "\":");
                json_stringify_node(n->object_val.pairs[i].value, buf, pos, cap);
            }
            json_buf_append(buf, pos, cap, "}");
            break;
    }
}

const char* chris_json_stringify(long long handle) {
    chris_json_node* n = (chris_json_node*)handle;
    if (!n) return "null";
    int cap = 65536;
    char* buf = (char*)malloc(cap);
    int pos = 0;
    json_stringify_node(n, buf, &pos, cap);
    buf[pos] = '\0';
    char* result = (char*)chris_gc_alloc(pos + 1, GC_STRING);
    memcpy(result, buf, pos + 1);
    free(buf);
    return result;
}
