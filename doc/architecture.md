# Joy Architecture Review

This document analyzes the current codebase structure and proposes improvements to address before adding new features.

## Current State Analysis

### Directory Structure

```
joy/
├── include/           # Headers (8 files)
│   ├── joy/          # Public API (joy.h)
│   ├── globals.h     # Core types, Env struct, macros (~500 lines)
│   ├── macros.h      # Node creation macros
│   ├── kvec.h        # Dynamic vector implementation
│   ├── khash.h       # Hash table implementation
│   └── ...
├── src/
│   ├── builtin/      # 229 .c files + 26 .h files (!)
│   ├── cli/          # Command-line interface (main.c)
│   ├── interp.c      # Interpreter core
│   ├── scan.c        # Scanner/lexer
│   ├── symbol.c      # Symbol table
│   └── ...           # ~16 core files
├── ext/              # External dependencies (linenoise)
├── lib/              # Joy library files (.joy)
├── tools/            # Build tools (gen_builtins.py)
└── tests/            # Test files
```

### Identified Issues

#### 1. Excessive File Fragmentation (src/builtin/)

**Problem**: 229 separate .c files for builtins, many under 500 bytes.

```
src/builtin/
├── abs.c      (547B)
├── acos.c     (201B)
├── plus.c     (315B)
├── minus.c    (236B)
└── ... (225 more)
```

**Impact**:
- Hard to navigate and understand relationships
- Build system complexity (code generation required)
- IDE performance issues with many small files
- Difficult to see patterns across related operations

**Example**: `plus.c` contains only:
```c
#ifndef PLUS_C
#define PLUS_C
PLUSMINUS(plus_, "+", +)
#endif
```

#### 2. Monolithic Env Struct

**Problem**: `Env` struct in `globals.h` is ~100 fields across multiple concerns.

```c
typedef struct Env {
    // Error handling (5 fields)
    jmp_buf error_jmp;
    char error_message[256];
    ...
    
    // Statistics (5 fields)
    double nodes, avail, collect, calls, opers;
    
    // Memory management (10+ fields)
    Node* memory;
    Index memoryindex, memorymax;
    ...
    
    // Scanner state (15+ fields)
    FILE* srcfile;
    int linenum, linepos;
    char linebuf[256];
    ...
    
    // Configuration flags (15+ fields)
    unsigned char autoput, echoflag, debugging;
    ...
    
    // I/O callbacks (struct)
    // ... more
} Env;
```

**Impact**:
- Hard to understand what's related
- Difficult to test components in isolation
- All state bundled together even when not needed

#### 3. Macro Complexity

**Problem**: Heavy reliance on macros makes code hard to follow.

```c
// In macros.h
#define NULLARY(CONSTRUCTOR, VALUE) env->stck = CONSTRUCTOR(VALUE, env->stck)
#define UNARY(CONSTRUCTOR, VALUE) env->stck = CONSTRUCTOR(VALUE, nextnode1(env->stck))

// In globals.h (differs between NOBDW and BDW!)
#ifdef NOBDW
#define nodetype(n) env->memory[n].op
#define nodevalue(n) env->memory[n].u
#else
#define nodetype(p) (p)->op
#define nodevalue(p) (p)->u
#endif
```

**Impact**:
- Macros assume `env` variable exists in scope
- Different behavior based on build configuration
- Hard to debug (macros don't show in stack traces)
- IDE code navigation doesn't work well

#### 4. Header Organization

**Problem**: `globals.h` includes too much:
- Standard library headers (stdio, stdlib, string, etc.)
- Platform-specific headers (windows.h, unistd.h)
- GC headers
- Hash table implementations
- Type definitions
- Macro definitions
- Function declarations

**Impact**:
- Every file pulls in everything
- Slow compilation
- Tight coupling

#### 5. Inconsistent Naming

**Problem**: Mixed naming conventions:
- Functions: `exeterm`, `getsym`, `newnode`, `my_memoryindex`
- Macros: `NULLARY`, `INTEGER_NEWNODE`, `POP`
- Types: `pEnv`, `proc_t`, `Index`

---

## Proposed Improvements

### Phase 1: Low Risk / High Impact

#### 1.1 Group Related Builtins

Instead of 229 files, group by category:

```
src/builtin/
├── arithmetic.c      # plus, minus, times, divide, mod, abs, neg
├── comparison.c      # equal, less, greater, compare
├── stack.c           # dup, swap, pop, rot, roll
├── list.c            # cons, first, rest, concat, size, at
├── control.c         # if, ifte, branch, cond, case
├── combinators.c     # map, fold, filter, times, while
├── recursion.c       # linrec, binrec, genrec, primrec, tailrec
├── io.c              # put, get, putch, getch, fopen, fclose
├── string.c          # concat (string), size (string), chr, ord
├── math.c            # sin, cos, tan, sqrt, exp, log
├── set.c             # union, intersect, member
├── type.c            # integer, float, string, list predicates
├── system.c          # clock, argc, argv, getenv, system
└── internal.c        # __dump, __memoryindex, debug helpers
```

**Benefits**:
- ~15 files instead of 229
- Related code visible together
- Easier to find patterns and refactor
- Better IDE support

**Migration**: Keep individual files during transition, have grouped files include them.

#### 1.2 Extract Subsystems from Env

Create focused structs:

```c
// env_types.h
typedef struct EnvError {
    jmp_buf jmp;
    char message[256];
    int line, column;
} EnvError;

typedef struct EnvStats {
    double nodes, avail, collect, calls, opers;
} EnvStats;

typedef struct EnvScanner {
    FILE* srcfile;
    char* filename;
    int linenum, linepos;
    char linebuf[INPLINEMAX + 1];
    // ... scanner-specific state
} EnvScanner;

typedef struct EnvConfig {
    unsigned autoput : 1;
    unsigned echoflag : 1;
    unsigned debugging : 1;
    unsigned tracegc : 1;
    // ... flags
} EnvConfig;

typedef struct Env {
    EnvError error;
    EnvStats stats;
    EnvScanner scanner;
    EnvConfig config;
    // ... memory, stack, etc.
} Env;
```

**Benefits**:
- Clear separation of concerns
- Easier to understand each subsystem
- Can pass just what's needed to functions
- Path to testing subsystems in isolation

#### 1.3 Create Type-Safe Accessors

Replace some macros with inline functions:

```c
// node.h
static inline Operator node_type(const Env* env, Index n) {
#ifdef NOBDW
    return env->memory[n].op;
#else
    return n->op;
#endif
}

static inline Types node_value(const Env* env, Index n) {
#ifdef NOBDW
    return env->memory[n].u;
#else
    return n->u;
#endif
}

static inline Index node_next(const Env* env, Index n) {
#ifdef NOBDW
    return env->memory[n].next;
#else
    return n->next;
#endif
}
```

**Benefits**:
- Type checking at compile time
- Explicit `env` parameter (no hidden dependency)
- Better debugging (functions show in stack traces)
- IDE navigation works

### Phase 2: Medium Risk / Medium Impact

#### 2.1 Split globals.h

```
include/
├── joy/
│   └── joy.h         # Public API (unchanged)
├── internal/
│   ├── types.h       # Core types (Node, Types, Entry, Operator)
│   ├── env.h         # Env struct and related
│   ├── node.h        # Node accessors (inline functions)
│   ├── macros.h      # Remaining macros (NEWNODE, etc.)
│   ├── platform.h    # Platform detection, system includes
│   └── config.h      # Build configuration
└── globals.h         # Compatibility shim that includes all
```

#### 2.2 Standardize Naming

| Current | Proposed | Rule |
|---------|----------|------|
| `pEnv` | `Env*` | No typedef for pointers |
| `exeterm` | `exec_term` | snake_case for functions |
| `getsym` | `scan_symbol` | module_action pattern |
| `my_memoryindex` | `mem_get_index` | Consistent prefixes |
| `NULLARY` | `stack_push` | Lowercase for inline funcs |

#### 2.3 Document Macro Contracts

Add clear documentation for macros:

```c
/**
 * Push a new node onto the stack.
 * 
 * @requires env != NULL
 * @requires env is in scope (macro captures it)
 * @modifies env->stck
 * 
 * Example:
 *   NULLARY(INTEGER_NEWNODE, 42);  // Push 42 onto stack
 */
#define NULLARY(CONSTRUCTOR, VALUE) ...
```

### Phase 3: Higher Risk / Long Term

#### 3.1 Introduce Error Handling Pattern

Replace setjmp/longjmp with explicit error returns:

```c
typedef struct {
    JoyResult code;
    const char* message;
    int line, column;
} JoyError;

JoyError exec_term(Env* env, Index n);

// Usage:
JoyError err = exec_term(env, program);
if (err.code != JOY_OK) {
    report_error(err);
}
```

#### 3.2 Add Debug/Trace Infrastructure

```c
#ifdef JOY_DEBUG
#define TRACE(env, fmt, ...) joy_trace(env, __func__, fmt, ##__VA_ARGS__)
#else
#define TRACE(env, fmt, ...) ((void)0)
#endif
```

#### 3.3 Consider Code Generation Alternatives

Instead of Python-generated includes, consider:
- X-macros for operator tables
- Single builtin.c with all operations
- Compile-time registration

---

## Recommended Priority

### Before Adding New Features

1. **Group builtins** - Reduces cognitive load, improves navigation
2. **Extract EnvConfig** - Cleanest, lowest risk
3. **Add inline accessors** - Keep macros as fallback
4. **Document macro contracts** - Helps contributors

### During Feature Development

5. **Extract EnvScanner** - When adding new syntax
6. **Extract EnvError** - When improving error handling
7. **Split globals.h** - Gradual, file by file

### Long Term

8. **Standardize naming** - Big change, do incrementally
9. **Error handling pattern** - Major architectural change
10. **Code generation alternatives** - If build complexity becomes issue

---

## Files to Create

| File | Purpose |
|------|---------|
| `include/internal/types.h` | Core type definitions |
| `include/internal/node.h` | Node accessor functions |
| `include/internal/env.h` | Env struct (refactored) |
| `src/builtin/arithmetic.c` | Grouped arithmetic builtins |
| `src/builtin/stack.c` | Grouped stack builtins |
| ... | (other grouped builtins) |

---

## Metrics to Track

| Metric | Current | Target |
|--------|---------|--------|
| Files in src/builtin/ | 255 | ~20 |
| Lines in globals.h | ~500 | ~150 |
| Env struct fields | ~100 | Organized into sub-structs |
| Build warnings | ? | 0 |
| Test coverage | ? | >80% for core |
