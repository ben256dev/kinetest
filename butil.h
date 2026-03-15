#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>

#define log(fmt, ...)                                                           \
    do {                                                                        \
        fprintf(stderr, "%s:%d \x1b[1;31merror: %s(): \x1b[0m" fmt "\n",        \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__);                       \
    } while (0)

#define die(fmt, ...)                                                           \
    do {                                                                        \
        fflush(stdout);                                                         \
        log(fmt, ##__VA_ARGS__);                                                \
        exit(EXIT_FAILURE);                                                     \
    } while (0)

// Allocation macros that automatically check return values
static inline void *xmalloc(size_t n);
static inline void *xcalloc(size_t count, size_t size);
static inline void *xrealloc(void *ptr, size_t n);
static inline char *xstrdup(const char *s);

#ifdef BUTIL_IMPLEMENTATION
static inline void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p && n != 0)
        die("malloc(%zu) failed: %s", n, strerror(errno));
    return p;
}

static inline void *xcalloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size)
        die("calloc overflow: count=%zu size=%zu", count, size);

    void *p = calloc(count, size);
    if (!p && count != 0 && size != 0)
        die("calloc(%zu, %zu) failed: %s", count, size, strerror(errno));
    return p;
}

static inline void *xrealloc(void *ptr, size_t n)
{
    void *p = realloc(ptr, n);
    if (!p && n != 0)
        die("realloc(%p, %zu) failed: %s", ptr, n, strerror(errno));
    return p;
}

static inline char *xstrdup(const char *s)
{
    if (!s)
        die("strdup(NULL)");

    size_t n = strlen(s) + 1;
    char *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}
#endif

// Forbid Usage of std allocation functions
#ifndef BUTIL_ALLOW_STD_ALLOC
#ifdef malloc
#undef malloc
#endif
#ifdef calloc
#undef calloc
#endif
#ifdef realloc
#undef realloc
#endif
#ifdef strdup
#undef strdup
#endif

#define BUTIL_POISON(msg) \
    (0 * sizeof(struct { _Static_assert(0, msg); int _; }))

#define malloc(...)  BUTIL_POISON("Do not use malloc(); use xmalloc()")
#define calloc(...)  BUTIL_POISON("Do not use calloc(); use xcalloc()")
#define realloc(...) BUTIL_POISON("Do not use realloc(); use xrealloc()")
#define strdup(...)  BUTIL_POISON("Do not use strdup(); use xstrdup()")
#endif

/* Dynamic Array header idea from Dylan Falconer:
 * [bytesbeneath](https://www.bytesbeneath.com/p/dynamic-arrays-in-c) */
typedef struct {
	size_t    length;
	size_t    capacity;
	size_t    padding;
	size_t    item_size;
} Array_Header;

typedef void *(*Array_Realloc_Fun)(void *data, size_t src_bytes, size_t dst_bytes);

static void *array_default_realloc(void *data, size_t src_bytes, size_t dst_bytes);
static Array_Realloc_Fun array_realloc_fun = array_default_realloc;

static inline Array_Header *array_header(void *a)
{
    return (Array_Header *)a - 1;
}

#define ARRAY_INITIAL_CAPACITY 16

#define array(T) ((T *)array_init(sizeof(T), ARRAY_INITIAL_CAPACITY))
#define array_length(a) (array_header(a)->length)
#define array_capacity(a) (array_header(a)->capacity)

#define array_pop_back(a) do { \
    array_header(a)->length -= 1; \
} while (0)

#define array_append(a, v) ( \
	(a) = array_ensure_capacity((a), 1), \
	(a)[array_header(a)->length] = (v), \
	&(a)[array_header(a)->length++]) \

#define array_remove_unordered(a, i) do { \
    Array_Header *h = array_header(a); \
    size_t _i = (size_t)(i); \
    if (_i >= h->length) break; \
    size_t _last = h->length - 1; \
    if (_i != _last) { \
        (a)[_i] = (a)[_last]; \
    } \
    h->length -= 1; \
} while (0)

#define array_remove(a, i) do { \
    Array_Header *h = array_header(a); \
    size_t _i = (i); \
    if (h->length == 0 || _i >= h->length) break; \
    if (_i != h->length - 1) { \
        memmove(&(a)[_i], &(a)[_i + 1], (h->length - _i - 1) * sizeof(*(a))); \
    } \
    h->length -= 1; \
} while (0)

/* foreach macro idea comes from tsoding's implementation in his
 * [nob.h](https://github.com/tsoding/nob.h) library */
#define array_foreach_ptr(el, arr) \
    for (__typeof__(arr) it = arr, el; it < arr + array_length(arr) && (el = it, 1); ++it) \

#define array_foreach_const(el, arr) \
    for (const __typeof__(*arr) *it = arr; it < arr + array_length(arr); ++it) \
		for (const __typeof__(*arr) const *el = it, *loop = (void*)1; loop; loop = 0)

#define array_foreach(el, arr) \
    for (__typeof__(*arr) *it = arr, el = *arr; it < arr + array_length(arr) && (el = *it, 1); ++it)

void array_set_realloc(Array_Realloc_Fun fun_ptr);
void *array_init(size_t item_size, size_t capacity);
void array_free(void *a);
void *array_ensure_capacity(void *a, size_t item_count);

#ifdef BUTIL_IMPLEMENTATION
static void *array_default_realloc(void *data, size_t src_bytes, size_t dst_bytes)
{
    (void)src_bytes;

    if (dst_bytes == 0) {
        free(data);
        return NULL;
    }

    if (!data)
        return xmalloc(dst_bytes);

    return xrealloc(data, dst_bytes);
}

void array_set_realloc(Array_Realloc_Fun fun_ptr)
{
    array_realloc_fun = fun_ptr ? fun_ptr : array_default_realloc;
}

void *array_init(size_t item_size, size_t capacity)
{
    if (capacity == 0) capacity = 1;

    size_t bytes = sizeof(Array_Header) + item_size * capacity;
    Array_Header *h = array_realloc_fun(NULL, 0, bytes);
    if (!h) return NULL;

    h->length = 0;
    h->capacity = capacity;
    h->padding = 0;
    h->item_size = item_size;

    return (void *)(h + 1);
}

void array_free(void *a)
{
    if (!a) return;

    Array_Header *h = array_header(a);
    size_t old_bytes = sizeof(Array_Header) + h->item_size * h->capacity;
    array_realloc_fun(h, old_bytes, 0);
}

void *array_ensure_capacity(void *a, size_t item_count)
{
    Array_Header *h = array_header(a);
    size_t desired = h->length + item_count;

    if (h->capacity >= desired)
        return a;

    size_t new_capacity = h->capacity ? h->capacity : 1;
    while (new_capacity < desired)
        new_capacity *= 2;

    size_t old_bytes = sizeof(Array_Header) + h->item_size * h->capacity;
    size_t new_bytes = sizeof(Array_Header) + h->item_size * new_capacity;

    Array_Header *new_h = array_realloc_fun(h, old_bytes, new_bytes);
    if (!new_h)
        return NULL;

    new_h->capacity = new_capacity;
    return (void *)(new_h + 1);
}
#endif
