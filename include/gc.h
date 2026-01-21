/*
    module  : gc.h
    version : 1.25
    date    : 01/21/26

    Conservative garbage collector with context isolation for parallel execution.
*/
#ifndef GC_H
#define GC_H

#include <stddef.h>
#include <stdint.h>

/* Configuration options - must be before struct definition */
/* #define SCAN_BSS_MEMORY */
/* #define STACK_GROWS_UPWARD */
/* #define USE_GC_REALLOC */

#ifdef MALLOC_DEBUG
#define FREE_ON_EXIT
#endif
#define USE_GC_MALLOC_ATOMIC
#define USE_GC_MALLOC
#define USE_GC_STRDUP
#define COUNT_COLLECTIONS

/* Forward declaration for khash - actual type defined in gc.c */
struct kh_Backup_s;

/*
 * GC_Context - Per-context garbage collector state
 *
 * This structure contains all state needed for garbage collection,
 * allowing multiple independent GC contexts for parallel execution.
 */
typedef struct GC_Context {
    struct kh_Backup_s* mem;    /* hash table tracking allocations */
    uint32_t max_items;         /* threshold for triggering GC */
    uint64_t lower;             /* lower bound of heap pointers */
    uint64_t upper;             /* upper bound of heap pointers */
    char* stack_bottom;         /* bottom of C stack for this context */
#ifdef COUNT_COLLECTIONS
    size_t gc_no;               /* number of garbage collections */
    size_t memory_use;          /* bytes currently allocated */
    size_t free_bytes;          /* bytes freed by GC */
#endif
} GC_Context;

/* External dependencies */
extern char* bottom_of_stack;  /* global stack bottom for legacy API */
void fatal(char* str);

/*
 * Context-aware API (thread-safe)
 *
 * These functions operate on an explicit GC_Context, allowing
 * multiple independent garbage collectors for parallel execution.
 */
GC_Context* gc_ctx_create(void);
void gc_ctx_destroy(GC_Context* ctx);
void gc_ctx_set_stack_bottom(GC_Context* ctx, char* bottom);

void* gc_ctx_malloc(GC_Context* ctx, size_t size);
void* gc_ctx_malloc_atomic(GC_Context* ctx, size_t size);
void* gc_ctx_realloc(GC_Context* ctx, void* ptr, size_t size);
char* gc_ctx_strdup(GC_Context* ctx, const char* str);
void gc_ctx_collect(GC_Context* ctx);

size_t gc_ctx_get_gc_no(GC_Context* ctx);
size_t gc_ctx_get_memory_use(GC_Context* ctx);
size_t gc_ctx_get_free_bytes(GC_Context* ctx);

/*
 * Legacy API (uses global default context)
 *
 * These functions maintain backwards compatibility with existing code.
 * They use a single global GC_Context and are NOT thread-safe.
 * New code should use the context-aware API above.
 */
void GC_INIT(void);
void GC_gcollect(void);
void* GC_malloc_atomic(size_t size);
void* GC_malloc(size_t size);
void* GC_realloc(void* old, size_t size);
char* GC_strdup(const char* str);
size_t GC_get_gc_no(void);
size_t GC_get_memory_use(void);
size_t GC_get_free_bytes(void);

/*
 * Get the default global GC context.
 * Returns NULL if GC_INIT() has not been called.
 */
GC_Context* gc_get_default_context(void);

#endif
