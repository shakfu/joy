/*
    module  : gc.c
    version : 1.55
    date    : 01/21/26

    Conservative garbage collector with context isolation for parallel execution.
*/
#ifdef MALLOC_DEBUG
#include "rmalloc.h"
#endif

#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(disable : 4267)
#endif

#ifdef __linux__
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

#include "gc.h"
#include "khash.h"

#ifdef _MSC_VER
#define DOWN_64K ~0xFFFF
#define PEPOINTER 15
#define IMAGE_BASE 13
#define BASE_OF_CODE 11
#define SIZE_OF_CODE 7
#define SIZE_OF_DATA 8
#define SIZE_OF_BSS 9
#endif

#define GC_COLL 0
#define GC_LEAF 1
#define GC_MARK 2

#define GROW_FACTOR 2
#define BSS_ALIGN 4
#define MIN_ITEMS 170 /* initial number of items */

/*
 * When pointers are aligned at 16 bytes, the lower 4 bits are always zero.
 */
#define HASH_FUNCTION(key) (khint_t)((key) >> 4)

typedef struct mem_info {
    unsigned flags : 2;
    unsigned size : 30;
} mem_info;

/*
 * The map contains a pointer as key and mem_info as value.
 */
KHASH_INIT(Backup, uint64_t, mem_info, 1, HASH_FUNCTION, kh_int64_hash_equal)

/*
 * Default global context for legacy API.
 * Initialized by GC_INIT(), used by GC_malloc() etc.
 */
static GC_Context* default_ctx = NULL;

/*
 * Pointers to memory segments (global, used for BSS scanning).
 */
#ifdef SCAN_BSS_MEMORY
static uint64_t start_of_text, start_of_data, start_of_bss, start_of_heap;
#endif

/*
 * fatal - Report a fatal error and end the program.
 */
#ifdef _MSC_VER
void fatal(char* str);
#endif

/*
 * Determine sections of memory. This is highly system dependent.
 */
#ifdef SCAN_BSS_MEMORY
static void init_heap(void)
{
    extern int main(int argc, char** argv);
#ifdef _MSC_VER
    int* ptr;
#endif
    start_of_text = (uint64_t)main;
#ifdef __CYGWIN__
    extern char __data_start__, __bss_start__, __bss_end__;
    start_of_data = (uint64_t)&__data_start__;
    start_of_bss = (uint64_t)&__bss_start__;
    start_of_heap = (uint64_t)&__bss_end__;
#endif
#ifdef __linux__
    extern char etext, edata, end;
    start_of_data = (uint64_t)&etext;
    start_of_bss = (uint64_t)&edata;
    start_of_heap = (uint64_t)&end;
#endif
#ifdef __APPLE__
    start_of_data = (uint64_t)get_etext();
    start_of_bss = (uint64_t)get_edata();
    start_of_heap = (uint64_t)get_end();
#endif
#ifdef _MSC_VER
    start_of_text &= DOWN_64K;
    ptr = (int*)start_of_text;
    ptr += ptr[PEPOINTER] / 4;
    start_of_text = ptr[IMAGE_BASE] + ptr[BASE_OF_CODE];
    start_of_data = start_of_text + ptr[SIZE_OF_CODE];
    start_of_bss = start_of_data + ptr[SIZE_OF_DATA];
    start_of_heap = start_of_bss + ptr[SIZE_OF_BSS];
#endif
}
#endif

/* ========================================================================
 * Context-aware internal functions
 * ======================================================================== */

/*
 * Mark a block as in use. No optimization for this (recursive) function.
 */
static void ctx_mark_ptr(GC_Context* ctx, char* ptr)
{
    khint_t key;
    size_t i, size;
    uintptr_t value;
    khash_t(Backup)* mem = (khash_t(Backup)*)ctx->mem;

    value = (uintptr_t)ptr;
    if (value < ctx->lower || value >= ctx->upper)
        return;
    if ((key = kh_get(Backup, mem, value)) != kh_end(mem)) {
        if (kh_val(mem, key).flags & GC_MARK)
            return;
        kh_val(mem, key).flags |= GC_MARK;
        if (kh_val(mem, key).flags & GC_LEAF)
            return;
        size = kh_val(mem, key).size;
        if (value + size > ctx->upper)
            size = ctx->upper - value;
        size /= sizeof(char*);
        if (size == 0)
            return;
        for (i = 0; i < size; i++) {
            uintptr_t slot_addr = value + i * sizeof(char*);
            char *child;
            if (slot_addr + sizeof(char*) > ctx->upper)
                break;
            memcpy(&child, (void *)slot_addr, sizeof(char*));
            ctx_mark_ptr(ctx, child);
        }
    }
}

/*
 * Mark blocks that can be found on the stack.
 */
static void ctx_mark_stk(GC_Context* ctx)
{
    uint64_t ptr = (uint64_t)&ptr;
    char* stack_bottom = ctx->stack_bottom;

    if (!stack_bottom)
        return;  /* No stack bottom set, skip stack scanning */

#ifdef STACK_GROWS_UPWARD
    if (ptr > (uint64_t)stack_bottom)
        for (; ptr > (uint64_t)stack_bottom; ptr -= sizeof(char*))
            ctx_mark_ptr(ctx, *(char**)ptr);
    else
#endif
        for (; ptr < (uint64_t)stack_bottom; ptr += sizeof(char*))
            ctx_mark_ptr(ctx, *(char**)ptr);
}

/*
 * Mark blocks that are pointed to from static uninitialized memory.
 */
#ifdef SCAN_BSS_MEMORY
static void ctx_mark_bss(GC_Context* ctx)
{
    uint64_t ptr, end_of_bss;

    end_of_bss = start_of_heap - sizeof(void*);
    for (ptr = start_of_bss; ptr <= end_of_bss; ptr += BSS_ALIGN)
        ctx_mark_ptr(ctx, *(char**)ptr);
}
#endif

/*
 * Walk registered blocks and free those that have not been marked.
 */
static void ctx_scan(GC_Context* ctx)
{
    khint_t key;
    khash_t(Backup)* mem = (khash_t(Backup)*)ctx->mem;

    for (key = 0; key != kh_end(mem); key++) {
        if (kh_exist(mem, key)) {
            if (kh_val(mem, key).flags & GC_MARK)
                kh_val(mem, key).flags &= ~GC_MARK;
            else {
#ifdef COUNT_COLLECTIONS
                ctx->free_bytes += kh_val(mem, key).size;
#endif
                free((void*)kh_key(mem, key));
                kh_del(Backup, mem, key);
            }
        }
    }
}

/*
 * Register an allocated memory block and garbage collect if needed.
 */
static void ctx_remind(GC_Context* ctx, char* ptr, size_t size, int flags)
{
    int rv;
    khint_t key;
    uint64_t value;
    khash_t(Backup)* mem = (khash_t(Backup)*)ctx->mem;

    value = (uint64_t)ptr;
    if (ctx->lower > value || !ctx->lower)
        ctx->lower = value;
    if (ctx->upper < value + size)
        ctx->upper = value + size;
    key = kh_put(Backup, mem, value, &rv);
    kh_val(mem, key).flags = flags;
    kh_val(mem, key).size = size;
#ifdef COUNT_COLLECTIONS
    ctx->memory_use += size;
#endif
    /*
     * See if there are already too many items allocated. If yes, trigger the
     * garbage collector. The threshold is set to twice the number of items
     * currently in use, allowing 100% growth between collections.
     */
    if (ctx->max_items < kh_size(mem)) {
        gc_ctx_collect(ctx);
        ctx->max_items = kh_size(mem) * GROW_FACTOR;
    }
}

/*
 * Allocate and register a memory block. The block is cleared with zeroes.
 */
static void* ctx_mem_block(GC_Context* ctx, size_t size, int leaf)
{
    void* ptr = malloc(size);
#ifdef _MSC_VER
    if (!ptr)
        fatal("memory exhausted");
#endif
    if (ptr) {
        memset(ptr, 0, size);
        ctx_remind(ctx, ptr, size, leaf);
    }
    return ptr;
}

/*
 * Forget about a memory block and return its flags.
 */
#ifdef COUNT_COLLECTIONS
static unsigned char ctx_forget(GC_Context* ctx, void* ptr, unsigned* size)
#else
static unsigned char ctx_forget(GC_Context* ctx, void* ptr)
#endif
{
    khint_t key;
    unsigned char flags = 0;
    khash_t(Backup)* mem = (khash_t(Backup)*)ctx->mem;

    if ((key = kh_get(Backup, mem, (uint64_t)ptr)) != kh_end(mem)) {
        flags = kh_val(mem, key).flags;
#ifdef COUNT_COLLECTIONS
        *size = kh_val(mem, key).size;
#endif
        kh_del(Backup, mem, key);
    }
    return flags;
}

/* ========================================================================
 * Context-aware public API
 * ======================================================================== */

/*
 * Create a new GC context.
 */
GC_Context* gc_ctx_create(void)
{
    GC_Context* ctx = calloc(1, sizeof(GC_Context));
    if (!ctx)
        return NULL;

    ctx->mem = kh_init(Backup);
    if (!ctx->mem) {
        free(ctx);
        return NULL;
    }
    ctx->max_items = MIN_ITEMS;
    ctx->lower = 0;
    ctx->upper = 0;
    ctx->stack_bottom = NULL;
#ifdef COUNT_COLLECTIONS
    ctx->gc_no = 0;
    ctx->memory_use = 0;
    ctx->free_bytes = 0;
#endif
    return ctx;
}

/*
 * Destroy a GC context and free all tracked allocations.
 */
void gc_ctx_destroy(GC_Context* ctx)
{
    khint_t key;
    khash_t(Backup)* mem;

    if (!ctx)
        return;

    mem = (khash_t(Backup)*)ctx->mem;
    if (mem) {
        /* Free all tracked allocations */
        for (key = 0; key != kh_end(mem); key++)
            if (kh_exist(mem, key))
                free((void*)kh_key(mem, key));
        kh_destroy(Backup, mem);
    }
    free(ctx);
}

/*
 * Set the stack bottom for a context.
 */
void gc_ctx_set_stack_bottom(GC_Context* ctx, char* bottom)
{
    if (ctx)
        ctx->stack_bottom = bottom;
}

/*
 * Allocate memory that may contain pointers (will be scanned during GC).
 */
void* gc_ctx_malloc(GC_Context* ctx, size_t size)
{
    if (!ctx)
        return NULL;
    return ctx_mem_block(ctx, size, GC_COLL);
}

/*
 * Allocate memory that contains no pointers (leaf allocation).
 */
void* gc_ctx_malloc_atomic(GC_Context* ctx, size_t size)
{
    if (!ctx)
        return NULL;
    return ctx_mem_block(ctx, size, GC_LEAF);
}

/*
 * Reallocate a memory block.
 */
void* gc_ctx_realloc(GC_Context* ctx, void* ptr, size_t size)
{
    unsigned char flags;
#ifdef COUNT_COLLECTIONS
    unsigned old_size = 0;
#endif

    if (!ctx)
        return NULL;
    if (!ptr)
        return gc_ctx_malloc(ctx, size);

#ifdef COUNT_COLLECTIONS
    flags = ctx_forget(ctx, ptr, &old_size);
    ctx->memory_use -= old_size;
#else
    flags = ctx_forget(ctx, ptr);
#endif
    ptr = realloc(ptr, size);
#ifdef _MSC_VER
    if (!ptr)
        fatal("memory exhausted");
#endif
    if (ptr)
        ctx_remind(ctx, ptr, size, flags);
    return ptr;
}

/*
 * Duplicate a string.
 */
char* gc_ctx_strdup(GC_Context* ctx, const char* str)
{
    char* ptr;
    size_t leng;

    if (!ctx || !str)
        return NULL;
    leng = strlen(str) + 1;
    ptr = gc_ctx_malloc_atomic(ctx, leng);
    if (ptr)
        strcpy(ptr, str);
    return ptr;
}

/*
 * Collect garbage in a context.
 */
void gc_ctx_collect(GC_Context* ctx)
{
    jmp_buf env;
    void (*volatile m)(GC_Context*) = ctx_mark_stk;

    if (!ctx)
        return;

    memset(&env, 0, sizeof(jmp_buf));
    setjmp(env);
    (*m)(ctx);
#ifdef SCAN_BSS_MEMORY
    ctx_mark_bss(ctx);
#endif
    ctx_scan(ctx);
#ifdef COUNT_COLLECTIONS
    ctx->gc_no++;
#endif
}

/*
 * Get statistics from a context.
 */
size_t gc_ctx_get_gc_no(GC_Context* ctx)
{
#ifdef COUNT_COLLECTIONS
    return ctx ? ctx->gc_no : 0;
#else
    (void)ctx;
    return 0;
#endif
}

size_t gc_ctx_get_memory_use(GC_Context* ctx)
{
#ifdef COUNT_COLLECTIONS
    return ctx ? ctx->memory_use : 0;
#else
    (void)ctx;
    return 0;
#endif
}

size_t gc_ctx_get_free_bytes(GC_Context* ctx)
{
#ifdef COUNT_COLLECTIONS
    return ctx ? ctx->free_bytes : 0;
#else
    (void)ctx;
    return 0;
#endif
}

/*
 * Get the default global context.
 */
GC_Context* gc_get_default_context(void)
{
    return default_ctx;
}

/* ========================================================================
 * Legacy API (uses global default context)
 * ======================================================================== */

/*
 * Initialize the default global GC context.
 */
void GC_INIT(void)
{
#ifdef SCAN_BSS_MEMORY
    init_heap();
#endif
    if (!default_ctx) {
        default_ctx = gc_ctx_create();
        /* Set stack bottom from global for backwards compatibility */
        if (default_ctx)
            default_ctx->stack_bottom = bottom_of_stack;
    }
}

/*
 * Collect garbage using the default context.
 */
void GC_gcollect(void)
{
    /* Update stack bottom in case it changed */
    if (default_ctx)
        default_ctx->stack_bottom = bottom_of_stack;
    gc_ctx_collect(default_ctx);
}

/*
 * Allocate memory using the default context.
 */
#ifdef USE_GC_MALLOC_ATOMIC
void* GC_malloc_atomic(size_t size)
{
    return gc_ctx_malloc_atomic(default_ctx, size);
}
#endif

#ifdef USE_GC_MALLOC
void* GC_malloc(size_t size)
{
    return gc_ctx_malloc(default_ctx, size);
}
#endif

#ifdef USE_GC_REALLOC
void* GC_realloc(void* ptr, size_t size)
{
    return gc_ctx_realloc(default_ctx, ptr, size);
}
#endif

#ifdef USE_GC_STRDUP
char* GC_strdup(const char* str)
{
    return gc_ctx_strdup(default_ctx, str);
}
#endif

#ifdef COUNT_COLLECTIONS
size_t GC_get_gc_no(void)
{
    return gc_ctx_get_gc_no(default_ctx);
}

size_t GC_get_memory_use(void)
{
    return gc_ctx_get_memory_use(default_ctx);
}

size_t GC_get_free_bytes(void)
{
    return gc_ctx_get_free_bytes(default_ctx);
}
#endif
