#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>

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
