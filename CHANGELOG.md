# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- `PARALLEL.md` - Design document for adding concurrency/parallelism to Joy
- `include/parallel.h` - Infrastructure for parallel execution (env cloning, task structures, result copying)
- `pmap` combinator - Parallel map: `[list] [quotation] pmap -> [results]`
- `pfork` combinator - Parallel fork/join: `X [P1] [P2] pfork -> R1 R2`
- `JOY_PARALLEL` CMake option - Enable parallel combinators with OpenMP support
- `env->mem_low`, `env->memoryindex`, `env->memorymax` - Memory tracking fields moved to Env struct for context isolation
- `env->old_memory` - GC working memory pointer moved to Env struct
- `env->stack_bottom` - Per-context stack bound for future parallel GC support
- `GC_Context` struct - Per-context state for conservative GC (gc.c), enabling independent garbage collectors
- `gc_ctx_create()`, `gc_ctx_destroy()` - Create/destroy independent GC contexts
- `gc_ctx_malloc()`, `gc_ctx_malloc_atomic()`, `gc_ctx_realloc()`, `gc_ctx_strdup()` - Context-aware allocation functions
- `gc_ctx_collect()` - Trigger garbage collection on a specific context
- `gc_ctx_set_stack_bottom()` - Set stack bounds for context-specific stack scanning
- `gc_ctx_get_gc_no()`, `gc_ctx_get_memory_use()`, `gc_ctx_get_free_bytes()` - Context-aware statistics
- `gc_get_default_context()` - Access the global default GC context
- `env->gc_ctx` - Per-Joy-context conservative GC context pointer in Env struct
- `GC_CTX_MALLOC()`, `GC_CTX_MALLOC_ATOMIC()`, `GC_CTX_REALLOC()`, `GC_CTX_STRDUP()` - Convenience macros for context-aware allocation (fall back to global GC if `gc_ctx` is NULL)
- `check_malloc()` - Safe malloc wrapper with null pointer check
- `check_strdup()` - Safe strdup wrapper with null pointer check
- `count()` - Pre-calculates nodes needed before garbage collection
- `fatal()` - Fatal error handler in core library (error.c)
- `TEST_MALLOC_RETURN` define - Portable malloc return value checking (replaces `_MSC_VER` guards)
- `ALLOW_SYSTEM_CALLS` define - Explicit control over system() call availability

### Changed

#### GC Context Isolation for Parallel Execution

- **Phase 1 - NOBDW Copying GC**: Per-context state instead of global variables, enabling independent memory management for multiple Joy contexts
  - `utils.c` - Refactored `inimem1()`, `inimem2()`, `copy()`, `gc1()`, `gc2()`, `newnode()`, `count()`, `my_memoryindex()`, `my_memorymax()` to use `env->*` fields
  - `gc1()` now accepts `Index *l, Index *r` parameters to preserve list and next pointers during garbage collection
  - `newnode()` uses `count()` to ensure sufficient memory before triggering GC
  - `newnode2()` uses `check_strdup()` instead of raw strdup with manual null check

- **Phase 2 - Conservative GC API**: Context-aware API for gc.c - all internal functions now operate on explicit `GC_Context*` parameter
  - `gc.c` - Refactored internal functions (`ctx_mark_ptr()`, `ctx_mark_stk()`, `ctx_scan()`, `ctx_remind()`, `ctx_mem_block()`, `ctx_forget()`)
  - Legacy API (`GC_malloc()`, `GC_gcollect()`, etc.) now wraps context-aware API using global default context

- **Phase 3 - Per-Joy-Context Integration**: Added `gc_ctx` field to Env struct, enabling per-Joy-context conservative GC
  - `joy.c` - Creates per-context GC in `joy_create()`, destroys in `joy_destroy()`
  - `scan.c`, `symbol.c`, `module.c`, `interp.c`, `undefs.c`, `intern.c` - Updated to use `GC_CTX_*` macros
  - Macros fall back to global GC when `gc_ctx` is NULL for backward compatibility with CLI

- **Phase 4 - Parallel Combinators**: Added `pmap` and `pfork` combinators with OpenMP support
  - `include/parallel.h` - Infrastructure for parallel execution (env cloning, result copying)
  - `src/builtin/pmap.c` - Parallel map combinator (`[list] [quotation] pmap -> [results]`)
  - `src/builtin/pfork.c` - Parallel fork/join combinator (`X [P1] [P2] pfork -> R1 R2`)
  - `CMakeLists.txt` - Added `JOY_PARALLEL` option with OpenMP support (macOS via libomp, Linux native)

- **Phase 5 - Parallel Execution**: Implemented actual parallel execution using OpenMP
  - `pmap` uses `#pragma omp parallel for` for lists with 4+ elements
  - `pfork` uses `#pragma omp parallel sections` for concurrent branch execution
  - Each parallel task gets a cloned environment with isolated GC context
  - Deep copying of inputs/quotations to child contexts and results back to parent
  - Graceful fallback to sequential execution for small lists or allocation failures

#### Other Changes

- `inimem1()`, `gc1()`, `gc2()`, `newnode()` use `TEST_MALLOC_RETURN` instead of `_MSC_VER` for portable memory checking
- `scan.c` uses `ALLOW_SYSTEM_CALLS` instead of `WINDOWS_S` for shell escape control
- Test runner now loads `lib/inilib.joy` first to set up library search paths

### Fixed

- GC parameter preservation - parameters passed to `newnode()` are now safely copied during garbage collection
- Test infrastructure - tests using `libload` (numlib, etc.) now work correctly (178/178 tests pass)

### Upstream Sync

Core changes from [Wodan58/Joy](https://github.com/Wodan58/Joy) Patch Tuesday V (2026-01-13):
- Portable pointer handling with `pointer_t` type abstraction
- Improved memory safety with checked allocation wrappers
- GC improvements for parameter preservation during collection
- Configurable system call control

## [1.37] - 2026-01-20

### Added

- Tab completion in REPL
- Linenoise library for line editing

### Fixed

- REPL double-free bug

### Changed

- Restructured test output to build directory
- Moved source files to `src/` directory structure
- Added embedding examples in `examples/`
