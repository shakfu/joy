# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- `PARALLEL.md` - Design document for adding concurrency/parallelism to Joy
- `env->mem_low`, `env->memoryindex`, `env->memorymax` - Memory tracking fields moved to Env struct for context isolation
- `env->old_memory` - GC working memory pointer moved to Env struct
- `env->stack_bottom` - Per-context stack bound for future parallel GC support
- `check_malloc()` - Safe malloc wrapper with null pointer check
- `check_strdup()` - Safe strdup wrapper with null pointer check
- `count()` - Pre-calculates nodes needed before garbage collection
- `fatal()` - Fatal error handler in core library (error.c)
- `TEST_MALLOC_RETURN` define - Portable malloc return value checking (replaces `_MSC_VER` guards)
- `ALLOW_SYSTEM_CALLS` define - Explicit control over system() call availability

### Changed

- **GC Context Isolation (Phase 1)**: NOBDW copying GC now uses per-context state instead of global variables, enabling independent memory management for multiple Joy contexts
- `utils.c` - Refactored `inimem1()`, `inimem2()`, `copy()`, `gc1()`, `gc2()`, `newnode()`, `count()`, `my_memoryindex()`, `my_memorymax()` to use `env->*` fields instead of static/global variables
- `joy.c` - Updated `joy_destroy()` to handle per-context memory cleanup
- `gc1()` now accepts `Index *l, Index *r` parameters to preserve list and next pointers during garbage collection, preventing potential memory corruption
- `newnode()` uses `count()` to ensure sufficient memory before triggering GC
- `newnode2()` uses `check_strdup()` instead of raw strdup with manual null check
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
