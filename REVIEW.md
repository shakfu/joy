# Joy Interpreter: Architecture Review and Modernization Plan

## Executive Summary

This review analyzes the Joy interpreter codebase with the goal of restructuring it as a clean library/frontend separation following modern C best practices. The current implementation is a mature, well-functioning interpreter but has architectural coupling that prevents clean embedding as a library. This document identifies specific issues and proposes a minimal, well-structured implementation.

---

## 1. Current Architecture Analysis

### 1.1 High-Level Structure

```
joy/
  src/
    main.c          -- CLI frontend + error recovery (jmp_buf)
    interp.c        -- Core evaluation engine
    scan.c          -- Lexer (static global state)
    factor.c        -- Parser
    symbol.c        -- Symbol table operations
    module.c        -- Module/scope management
    repl.c          -- Read-eval-print loop
    error.c         -- Error reporting
    utils.c         -- Memory management (NOBDW)
    gc.c            -- GC interface
    write.c/print.c -- Output formatting
    builtin/        -- 253 primitive operations
  include/
    globals.h       -- Types, Env struct, function prototypes
    macros.h        -- Node construction macros
    runtime.h       -- Parameter validation macros
```

### 1.2 Data Flow

```
Input -> Scanner (scan.c) -> Parser (factor.c) -> Evaluator (interp.c) -> Output
                                                        |
                                                   Primitives (builtin/*.c)
```

### 1.3 Current Library Build

The CMakeLists.txt already produces `joycore_static` and `joycore_shared`, but these cannot be cleanly embedded because:

1. `abortexecution_()` in main.c is called throughout the codebase
2. `bottom_of_stack` global is defined in main.c but used by gc.c
3. Scanner uses static global FILE* and line buffers
4. Error handling relies on `longjmp` to main.c's `jmp_buf begin`

---

## 2. Identified Issues

### 2.1 Critical: Error Recovery Coupled to main.c

**Location**: `src/main.c:120`, `src/main.c:133-137`

```c
static jmp_buf begin;  // In main.c

void abortexecution_(int num) {
    fflush(stdin);
    longjmp(begin, num);  // Jumps to main.c
}
```

This function is called from:
- `error.c:55` (`execerror`)
- `scan.c:54` (EOF handling)
- Various builtins (abort, quit)

**Problem**: Library users cannot catch errors; execution jumps back to a `setjmp` in main.c.

### 2.2 Critical: Global State in Scanner

**Location**: `src/scan.c:26-37`

```c
static FILE* srcfile;
static char* filenam;
static int linenum, linepos;
static char linebuf[INPLINEMAX + 1];
static struct { FILE* fp; int line; char name[...]; } infile[INPSTACKMAX];
static int ilevel = -1;
```

**Problem**: Only one interpreter instance can run at a time. No thread safety. State persists across calls.

### 2.3 Critical: GC Root in main.c

**Location**: `src/main.c:122`

```c
char* bottom_of_stack;  // Needed by gc.c for stack scanning
```

**Problem**: Memory management depends on main.c.

### 2.4 Moderate: Macro-Heavy Validation

**Location**: `include/runtime.h`

The 300+ lines of validation macros (ONEPARAM, TWOPARAMS, INTEGER, etc.) make debugging difficult and errors appear to come from the wrong source location.

### 2.5 Moderate: Direct stdout/stderr Usage

**Location**: Throughout codebase

Functions like `print()`, `writeterm()`, `error()` write directly to stdout/stderr instead of using configurable output streams.

### 2.6 Minor: Monolithic globals.h

**Location**: `include/globals.h` (417 lines)

The `Env` struct (60+ fields) mixes:
- Interpreter state (stck, prog, symtab)
- Configuration (autoput, echoflag, tracegc)
- CLI state (g_argc, g_argv, filename)
- Statistics (nodes, calls, opers)
- Memory management (memory, gc_clock)

### 2.7 Minor: Conditional Compilation Complexity

Flags like `NOBDW`, `BDW`, `COMPILER`, `BYTECODE`, `TRACEGC` create many code paths.

---

## 3. Proposed Architecture

### 3.1 Layer Separation

```
+------------------+
|   joy (CLI)      |  <- Frontend: argument parsing, terminal, REPL
+------------------+
         |
+------------------+
|   libjoy         |  <- Library: interpreter, primitives, memory
+------------------+
```

### 3.2 New Directory Structure

```
joy/
  include/
    joy/
      joy.h           -- Public API (embed this header only)
      types.h         -- Public types (JoyValue, JoyError)
      config.h        -- Build configuration
  src/
    lib/
      interpreter.c   -- Core evaluation
      scanner.c       -- Lexer (instance-based)
      parser.c        -- Parser
      symbols.c       -- Symbol table
      memory.c        -- Allocator/GC
      primitives.c    -- Generated builtin dispatch
      error.c         -- Error handling (returns, not longjmp)
    cli/
      main.c          -- CLI frontend
      repl.c          -- Interactive loop
      terminal.c      -- Raw mode handling
  primitives/         -- Renamed from src/builtin/
    *.c
```

### 3.3 Public API Design

```c
/* joy/joy.h - Public API */

#ifndef JOY_H
#define JOY_H

#include <stdint.h>
#include <stdio.h>

/* Opaque interpreter handle */
typedef struct JoyContext JoyContext;

/* Result codes */
typedef enum {
    JOY_OK = 0,
    JOY_ERROR_SYNTAX,
    JOY_ERROR_RUNTIME,
    JOY_ERROR_TYPE,
    JOY_ERROR_STACK_UNDERFLOW,
    JOY_ERROR_OUT_OF_MEMORY,
    JOY_ERROR_IO,
} JoyResult;

/* Value types */
typedef enum {
    JOY_TYPE_NIL = 0,
    JOY_TYPE_BOOLEAN,
    JOY_TYPE_INTEGER,
    JOY_TYPE_FLOAT,
    JOY_TYPE_CHAR,
    JOY_TYPE_STRING,
    JOY_TYPE_LIST,
    JOY_TYPE_SET,
    JOY_TYPE_FILE,
    JOY_TYPE_SYMBOL,
    JOY_TYPE_QUOTATION,
} JoyType;

/* Value representation (opaque or concrete depending on build) */
typedef struct JoyValue JoyValue;

/* Callbacks for I/O */
typedef struct {
    void* user_data;
    int  (*read_char)(void* user_data);
    void (*write_char)(void* user_data, int ch);
    void (*write_string)(void* user_data, const char* s);
    void (*error)(void* user_data, JoyResult code, const char* msg);
} JoyIO;

/* Configuration */
typedef struct {
    size_t initial_heap_size;   /* 0 = default */
    size_t max_heap_size;       /* 0 = unlimited */
    int    enable_gc_trace;     /* Debug output */
    JoyIO* io;                  /* NULL = use stdio */
} JoyConfig;

/* Lifecycle */
JoyContext* joy_create(const JoyConfig* config);
void        joy_destroy(JoyContext* ctx);

/* Evaluation */
JoyResult joy_eval_string(JoyContext* ctx, const char* source);
JoyResult joy_eval_file(JoyContext* ctx, FILE* fp, const char* filename);
JoyResult joy_eval_term(JoyContext* ctx, JoyValue term);

/* Stack access */
size_t    joy_stack_depth(JoyContext* ctx);
JoyValue  joy_stack_top(JoyContext* ctx);
JoyValue  joy_stack_at(JoyContext* ctx, size_t index);
JoyResult joy_push(JoyContext* ctx, JoyValue value);
JoyResult joy_pop(JoyContext* ctx, JoyValue* out);

/* Value construction */
JoyValue joy_nil(void);
JoyValue joy_boolean(int b);
JoyValue joy_integer(int64_t n);
JoyValue joy_float(double d);
JoyValue joy_char(int c);
JoyValue joy_string(JoyContext* ctx, const char* s);
JoyValue joy_list(JoyContext* ctx, JoyValue* items, size_t count);

/* Value inspection */
JoyType   joy_typeof(JoyValue v);
int       joy_is_nil(JoyValue v);
int64_t   joy_to_integer(JoyValue v);
double    joy_to_float(JoyValue v);
const char* joy_to_string(JoyContext* ctx, JoyValue v);

/* Symbol table */
JoyResult joy_define(JoyContext* ctx, const char* name, JoyValue body);
JoyValue  joy_lookup(JoyContext* ctx, const char* name);

/* Error information */
const char* joy_error_message(JoyContext* ctx);
int         joy_error_line(JoyContext* ctx);
int         joy_error_column(JoyContext* ctx);

/* Library loading */
JoyResult joy_load_stdlib(JoyContext* ctx);

#endif /* JOY_H */
```

---

## 4. Required Changes

### 4.1 Move Error Recovery to Context

**Current** (main.c):
```c
static jmp_buf begin;
void abortexecution_(int num) { longjmp(begin, num); }
```

**Proposed** (error.c):
```c
/* In JoyContext struct */
struct JoyContext {
    jmp_buf error_recovery;
    JoyResult last_error;
    char error_message[256];
    int error_line, error_column;
    /* ... */
};

/* Internal error function */
void joy_raise(JoyContext* ctx, JoyResult code, const char* msg) {
    ctx->last_error = code;
    strncpy(ctx->error_message, msg, sizeof(ctx->error_message) - 1);
    if (ctx->io && ctx->io->error) {
        ctx->io->error(ctx->io->user_data, code, msg);
    }
    longjmp(ctx->error_recovery, code);
}

/* Public eval with error boundary */
JoyResult joy_eval_string(JoyContext* ctx, const char* source) {
    int err = setjmp(ctx->error_recovery);
    if (err != 0) {
        return (JoyResult)err;
    }
    /* ... parsing and evaluation ... */
    return JOY_OK;
}
```

### 4.2 Instance-Based Scanner

**Current** (scan.c):
```c
static FILE* srcfile;
static int linenum, linepos;
```

**Proposed** (scanner.c):
```c
typedef struct {
    JoyContext* ctx;
    const char* source;      /* For string input */
    size_t source_pos;
    FILE* file;              /* For file input */
    char* filename;
    int line, column;
    char line_buffer[256];
    int pushback[16];
    int pushback_count;
} JoyScanner;

void scanner_init_string(JoyScanner* s, JoyContext* ctx, const char* source);
void scanner_init_file(JoyScanner* s, JoyContext* ctx, FILE* fp, const char* name);
int scanner_getch(JoyScanner* s);
void scanner_ungetch(JoyScanner* s, int ch);
JoyToken scanner_next(JoyScanner* s);
```

### 4.3 Configurable I/O

**Current** (write.c):
```c
void writefactor(pEnv env, Index n, FILE* fp) {
    fprintf(fp, "...");
}
```

**Proposed**:
```c
void joy_write_value(JoyContext* ctx, JoyValue v) {
    if (ctx->io && ctx->io->write_string) {
        char buf[64];
        /* format to buf */
        ctx->io->write_string(ctx->io->user_data, buf);
    } else {
        /* default to stdout */
    }
}
```

### 4.4 Refactor Env Struct

**Current** (globals.h):
```c
typedef struct Env {
    /* 60+ fields mixed together */
} Env;
```

**Proposed**:
```c
/* Internal state, not exposed in public header */
typedef struct JoyStack {
    Node* top;
    size_t depth;
} JoyStack;

typedef struct JoySymbolTable {
    vector(Entry) entries;
    khash_t(Symtab) hash;
} JoySymbolTable;

typedef struct JoyMemory {
    Node* heap;
    size_t heap_size;
    size_t used;
    /* GC state */
} JoyMemory;

struct JoyContext {
    /* Core interpreter state */
    JoyStack stack;
    JoySymbolTable symbols;
    JoyMemory memory;

    /* Error handling */
    jmp_buf error_recovery;
    JoyResult last_error;
    char error_message[256];

    /* Configuration */
    JoyIO* io;
    unsigned flags;  /* Bitfield for options */

    /* Statistics */
    struct {
        uint64_t nodes_allocated;
        uint64_t gc_collections;
        uint64_t operations;
    } stats;
};
```

---

## 5. Modern C Idioms to Adopt

### 5.1 Opaque Types for Encapsulation

```c
/* Public header */
typedef struct JoyContext JoyContext;  /* Opaque */

/* Private implementation */
struct JoyContext { /* ... */ };
```

### 5.2 Result Types Instead of setjmp for Non-Fatal Errors

```c
typedef struct {
    JoyResult code;
    JoyValue value;
} JoyEvalResult;

JoyEvalResult joy_try_eval(JoyContext* ctx, const char* source);
```

### 5.3 Const Correctness

```c
JoyValue joy_string(JoyContext* ctx, const char* s);  /* Does not modify s */
const char* joy_to_string(JoyContext* ctx, JoyValue v);  /* Returns read-only */
```

### 5.4 Explicit Sizing with size_t and Fixed-Width Integers

```c
int64_t joy_to_integer(JoyValue v);
size_t joy_stack_depth(JoyContext* ctx);
```

### 5.5 Inline Functions Over Macros Where Possible

**Current**:
```c
#define ONEPARAM(NAME) \
    if (!env->stck) { execerror(env, "one parameter", NAME); return; }
```

**Proposed**:
```c
static inline bool joy_check_stack(JoyContext* ctx, size_t n, const char* op) {
    if (joy_stack_depth(ctx) < n) {
        joy_raise(ctx, JOY_ERROR_STACK_UNDERFLOW, op);
        return false;
    }
    return true;
}
```

### 5.6 Designated Initializers (C99+)

```c
JoyConfig config = {
    .initial_heap_size = 1024 * 1024,
    .max_heap_size = 0,
    .enable_gc_trace = 0,
    .io = NULL,
};
```

### 5.7 Static Assertions for Compile-Time Checks

```c
_Static_assert(sizeof(JoyValue) <= 16, "JoyValue should fit in 16 bytes");
```

### 5.8 Restrict Pointers Where Applicable

```c
void joy_memcpy(void* restrict dst, const void* restrict src, size_t n);
```

---

## 6. Simplification Opportunities

### 6.1 Remove Conditional Compilation Where Feasible

Consider removing or making runtime-configurable:
- `BYTECODE` - can be a separate optional module
- `COMPILER` - can be a separate optional module
- Keep `NOBDW` as the default/only memory model for simplicity

### 6.2 Reduce Primitive Count

253 primitives could be reduced by:
- Consolidating type-specific variants (e.g., unify numeric operations)
- Moving complex combinators to the standard library
- Making some internal-only (prefixed with `__`)

### 6.3 Simplify Module System

The current HIDE/PRIVATE/PUBLIC/MODULE system is complex. Consider whether a simpler scoping mechanism would suffice.

---

## 7. Implementation Phases

### Phase 1: Decouple Error Handling (Critical)
1. Move `jmp_buf` into context structure
2. Create internal `joy_raise()` function
3. Update all error sites to use context
4. Add error boundary in public eval functions

### Phase 2: Instance-Based Scanner
1. Create `JoyScanner` structure
2. Move all static variables into scanner
3. Pass scanner through parsing functions
4. Support both string and file sources

### Phase 3: Extract Library
1. Create `include/joy/joy.h` public header
2. Move CLI code to `src/cli/`
3. Create minimal `joy.c` that implements public API
4. Update CMakeLists.txt for new structure

### Phase 4: I/O Abstraction
1. Define callback interface
2. Update output functions to use callbacks
3. Update input functions to use callbacks
4. Add default stdio implementation

### Phase 5: Polish
1. Documentation
2. Example embedding code
3. Test suite for library API
4. Version numbering

---

## 8. Compatibility Considerations

### 8.1 Source Compatibility

The Joy language itself remains unchanged. All existing `.joy` files should work.

### 8.2 Build Compatibility

- Existing build with `cmake` should continue to work
- New library targets: `libjoy.a`, `libjoy.so`/`libjoy.dylib`
- Header installation: `include/joy/joy.h`

### 8.3 Migration Path

The CLI `joy` binary can initially be a thin wrapper:

```c
int main(int argc, char** argv) {
    JoyContext* ctx = joy_create(NULL);
    if (!ctx) return 1;

    joy_load_stdlib(ctx);

    if (argc > 1) {
        FILE* fp = fopen(argv[1], "r");
        if (fp) {
            joy_eval_file(ctx, fp, argv[1]);
            fclose(fp);
        }
    } else {
        /* REPL loop */
        char line[256];
        while (fgets(line, sizeof(line), stdin)) {
            JoyResult r = joy_eval_string(ctx, line);
            if (r != JOY_OK) {
                fprintf(stderr, "Error: %s\n", joy_error_message(ctx));
            }
        }
    }

    joy_destroy(ctx);
    return 0;
}
```

---

## 9. Metrics for Success

1. **Embedability**: Can create multiple independent JoyContext instances
2. **Thread Safety**: Each context usable from a single thread (thread-local)
3. **Error Recovery**: Errors return results, not abort/longjmp out of caller
4. **Minimal API**: < 30 public functions
5. **No Global State**: All state in JoyContext
6. **Clean Headers**: Single `#include <joy/joy.h>` for embedders
7. **Backwards Compatible**: Existing `.joy` programs work unchanged

---

## 10. Open Questions

1. **GC Model**: Keep NOBDW (copying collector) only, or support both? NOBDW is simpler and self-contained.

2. **Bignum Support**: Currently incomplete (`BIGNUM_` type exists but implementation is partial). Remove or complete?

3. **File Handles**: Should the library manage file handles, or delegate to the embedder via callbacks?

4. **Module System**: Is the full MODULE/HIDE/PRIVATE/PUBLIC system necessary for a minimal implementation?

5. **Bytecode/Compiler**: These appear to be optional features. Include in core library or as separate modules?

---

## Conclusion

The Joy interpreter is a well-structured implementation with clear separation of lexing, parsing, and evaluation. The main barrier to library use is the tight coupling with main.c for error recovery and the global state in the scanner. These can be addressed with targeted refactoring while preserving the interpreter's functionality.

The proposed architecture maintains the elegant simplicity of the original design while enabling clean embedding. The phased implementation allows incremental progress with testable milestones.
